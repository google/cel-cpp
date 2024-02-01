// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/memory.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <new>  // IWYU pragma: keep
#include <ostream>

#include "absl/base/no_destructor.h"
#include "common/native_type.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "absl/base/attributes.h"
#include "absl/base/config.h"  // IWYU pragma: keep
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/log/die_if_null.h"
#include "absl/numeric/bits.h"

#ifdef ABSL_HAVE_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION(p, n)
#define ASAN_UNPOISON_MEMORY_REGION(p, n)
#endif

namespace cel {

namespace {

static_assert(sizeof(char) == 1);

size_t GetPageSize() {
  static const size_t page_size = []() -> size_t {
#ifdef _WIN32
    SYSTEM_INFO system_info;
    std::memset(&system_info, '\0', sizeof(system_info));
    ::GetSystemInfo(&system_info);
    return system_info.dwPageSize;
#else
#if defined(__wasm__) || defined(__asmjs__)
    return static_cast<size_t>(::getpagesize());
#else
    return static_cast<size_t>(::sysconf(_SC_PAGESIZE));
#endif
#endif
  }();
  return page_size;
}

uintptr_t AlignmentMask(size_t align) {
  ABSL_DCHECK(absl::has_single_bit(align));  // Assert aligned to power of 2.
  return align - size_t{1};
}

bool IsAligned(uintptr_t size, size_t align) {
  ABSL_DCHECK_NE(size, 0);
  ABSL_DCHECK(absl::has_single_bit(align));  // Assert aligned to power of 2.
  return (size & AlignmentMask(align)) == 0;
}

template <typename T>
bool IsAligned(T* pointer, size_t align) {
  return IsAligned(reinterpret_cast<uintptr_t>(pointer), align);
}

uintptr_t AlignDown(uintptr_t size, size_t align) {
  ABSL_DCHECK_NE(size, 0);
  ABSL_DCHECK(absl::has_single_bit(align));  // Assert aligned to power of 2.
  return size & ~AlignmentMask(align);
}

template <typename T>
T* AlignDown(T* pointer, size_t align) {
  return reinterpret_cast<T*>(
      AlignDown(reinterpret_cast<uintptr_t>(pointer), align));
}

uintptr_t AlignUp(uintptr_t size, size_t align) {
  ABSL_DCHECK_NE(size, 0);
  ABSL_DCHECK(absl::has_single_bit(align));  // Assert aligned to power of 2.
  return AlignDown(size + AlignmentMask(align), align);
}

template <typename T>
T* AlignUp(T* pointer, size_t align) {
  return reinterpret_cast<T*>(
      AlignUp(reinterpret_cast<uintptr_t>(pointer), align));
}

struct CleanupAction final {
  void* pointer;
  void (*destruct)(void*);
};

struct Region final {
  static Region* Create(size_t size, Region* prev) {
    return ::new (::operator new(size + sizeof(Region))) Region(size, prev);
  }

  const size_t size;
  Region* const prev;

  Region(size_t size, Region* prev) noexcept : size(size), prev(prev) {
    ASAN_POISON_MEMORY_REGION(reinterpret_cast<void*>(begin()), size);
  }

  uintptr_t begin() const noexcept {
    return reinterpret_cast<uintptr_t>(this) + sizeof(Region);
  }

  uintptr_t end() const noexcept { return begin() + size; }

  bool Contains(uintptr_t address) const noexcept {
    return address >= begin() && address < end();
  }

  void Destroy() noexcept {
    ASAN_UNPOISON_MEMORY_REGION(reinterpret_cast<void*>(begin()), size);
    void* const address = this;
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
    const auto total_size = size + sizeof(Region);
#endif
    this->~Region();
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
    ::operator delete(address, total_size);
#else
    ::operator delete(address);
#endif
  }
};

ABSL_ATTRIBUTE_NORETURN void ThrowStdBadAlloc() {
#ifdef ABSL_HAVE_EXCEPTIONS
  throw std::bad_alloc();
#else
  ABSL_LOG(FATAL) << "std::bad_alloc";
#endif
}

constexpr bool IsSizeTooLarge(size_t size) {
  return size > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max());
}

bool IsAlignmentTooLarge(size_t alignment) { return alignment > GetPageSize(); }

class ThreadCompatiblePoolingMemoryManager final : public PoolingMemoryManager {
 public:
  ~ThreadCompatiblePoolingMemoryManager() override {
    while (!cleanup_actions_.empty()) {
      auto cleanup_action = cleanup_actions_.front();
      cleanup_actions_.pop_front();
      (*cleanup_action.destruct)(cleanup_action.pointer);
    }
    auto* last = last_;
    while (last != nullptr) {
      auto* prev = last->prev;
      last->Destroy();
      last = prev;
    }
  }

 private:
  size_t CalculateRegionSize(size_t min_capacity) const {
    if (min_capacity <= min_region_size_) {
      return min_region_size_;
    }
    if (min_capacity >= max_region_size_) {
      return min_capacity;
    }
    size_t capacity = min_region_size_;
    while (capacity < min_capacity) {
      capacity *= 2;
    }
    return capacity;
  }

  absl::Nonnull<void*> AllocateImpl(size_t size, size_t align) override {
    ABSL_DCHECK_NE(size, 0);
    ABSL_DCHECK(absl::has_single_bit(align));
    if (ABSL_PREDICT_FALSE(IsSizeTooLarge(size))) {
      ThrowStdBadAlloc();
    }
    if (ABSL_PREDICT_FALSE(IsAlignmentTooLarge(align))) {
      ThrowStdBadAlloc();
    }
    ABSL_ATTRIBUTE_UNUSED auto prev = prev_;
    prev_ = next_;
#ifdef ABSL_HAVE_EXCEPTIONS
    try {
#endif
      if (ABSL_PREDICT_FALSE(next_ == 0)) {
        // Allocate first region.
        ABSL_DCHECK(first_ == nullptr);
        ABSL_DCHECK(last_ == nullptr);
        const size_t capacity =
            CalculateRegionSize(AlignUp(size + sizeof(Region), align));
        first_ = last_ = Region::Create(capacity - sizeof(Region), nullptr);
        prev_ = next_ = last_->begin();
      }
      uintptr_t address = AlignUp(next_, align);
      if (ABSL_PREDICT_FALSE(address < next_ || address >= last_->end() ||
                             last_->end() - address < size)) {
        // Allocate new region.
        const size_t capacity =
            CalculateRegionSize(AlignUp(size + sizeof(Region), align));
        min_region_size_ = std::min(min_region_size_ * 2, max_region_size_);
        last_ = Region::Create(capacity - sizeof(Region), last_);
        address = AlignUp(last_->begin(), align);
      }
      void* pointer = reinterpret_cast<void*>(address);
      ABSL_DCHECK(IsAligned(pointer, align));
      next_ = address + size;
      ASAN_UNPOISON_MEMORY_REGION(reinterpret_cast<void*>(address), size);
      return pointer;
#ifdef ABSL_HAVE_EXCEPTIONS
    } catch (...) {
      prev_ = prev;
      throw;
    }
#endif
  }

  bool DeallocateImpl(absl::Nonnull<void*> pointer, size_t size,
                      size_t align) noexcept override {
    ABSL_DCHECK(absl::has_single_bit(align));
    ABSL_DCHECK_NE(size, 0);
    ABSL_DCHECK(IsAligned(pointer, align));
    ABSL_DCHECK(!IsSizeTooLarge(size));
    ABSL_DCHECK(!IsAlignmentTooLarge(align));
    auto address = reinterpret_cast<uintptr_t>(pointer);
    ABSL_DCHECK(address != 0);
    if (next_ == 0 || prev_ == 0 || next_ == prev_ || address + size != next_) {
      return false;
    }
    if (!last_->Contains(prev_)) {
      auto* second_to_last = ABSL_DIE_IF_NULL(last_->prev);  // Crash OK
      ABSL_CHECK(second_to_last->Contains(prev_));           // Crash OK
      last_->Destroy();
      last_ = second_to_last;
    }
    next_ = prev_;
    ASAN_POISON_MEMORY_REGION(reinterpret_cast<void*>(next_),
                              last_->end() - next_);
    return true;
  }

  void OwnCustomDestructorImpl(
      void* object, absl::Nonnull<void (*)(void*)> destruct) override {
    ABSL_DCHECK(object != nullptr);
    ABSL_DCHECK(destruct != nullptr);
    cleanup_actions_.push_back(CleanupAction{object, destruct});
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<ThreadCompatiblePoolingMemoryManager>();
  }

  uintptr_t next_ = 0;
  uintptr_t prev_ = 0;
  Region* last_ = nullptr;
  std::deque<CleanupAction> cleanup_actions_;
  Region* first_ = nullptr;
  // Currently we use the same constants that protobuf does for their arena. We
  // could allow these to be tunable.
  size_t min_region_size_ = 256;
  const size_t max_region_size_ = 32768;
};

class UnreachablePoolingMemoryManager final : public PoolingMemoryManager {
 private:
  absl::Nonnull<void*> AllocateImpl(size_t, size_t) override {
    ABSL_LOG(FATAL) << "MemoryManager used after being moved";
  }

  bool DeallocateImpl(absl::Nonnull<void*>, size_t, size_t) noexcept override {
    ABSL_LOG(FATAL) << "MemoryManager used after being moved";
  }

  void OwnCustomDestructorImpl(void*, absl::Nonnull<void (*)(void*)>) override {
    ABSL_LOG(FATAL) << "MemoryManager used after being moved";
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<UnreachablePoolingMemoryManager>();
  }
};

struct UnmanagedPoolingMemoryManager {
  UnmanagedPoolingMemoryManager() = default;
  UnmanagedPoolingMemoryManager(const UnmanagedPoolingMemoryManager&) = delete;
  UnmanagedPoolingMemoryManager(UnmanagedPoolingMemoryManager&&) = delete;
  UnmanagedPoolingMemoryManager& operator=(
      const UnmanagedPoolingMemoryManager&) = delete;
  UnmanagedPoolingMemoryManager& operator=(UnmanagedPoolingMemoryManager&&) =
      delete;
};

absl::Nonnull<void*> UnmanagedPoolingMemoryManagerAllocate(absl::Nonnull<void*>,
                                                           size_t size,
                                                           size_t align) {
  if (align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
    return ::operator new(size);
  }
  return ::operator new(size, static_cast<std::align_val_t>(align));
}

bool UnmanagedPoolingMemoryManagerDeallocate(absl::Nonnull<void*>,
                                             absl::Nonnull<void*> ptr,
                                             size_t size,
                                             size_t alignment) noexcept {
  if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
    ::operator delete(ptr, size);
#else
    ::operator delete(ptr);
#endif
  } else {
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
    ::operator delete(ptr, size, static_cast<std::align_val_t>(alignment));
#else
    ::operator delete(ptr, static_cast<std::align_val_t>(alignment));
#endif
  }
  return true;
}

void UnmanagedPoolingMemoryManagerOwnCustomDestructor(
    absl::Nonnull<void*>, void*, absl::Nonnull<void (*)(void*)>) {}

const PoolingMemoryManagerVirtualTable& UnmanagedMemoryManagerVirtualTable() {
  static const PoolingMemoryManagerVirtualTable vtable{
      NativeTypeId::For<UnmanagedPoolingMemoryManager>(),
      &UnmanagedPoolingMemoryManagerAllocate,
      &UnmanagedPoolingMemoryManagerDeallocate,
      &UnmanagedPoolingMemoryManagerOwnCustomDestructor};
  return vtable;
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         MemoryManagement memory_management) {
  switch (memory_management) {
    case MemoryManagement::kPooling:
      return out << "POOLING";
    case MemoryManagement::kReferenceCounting:
      return out << "REFERENCE_COUNTING";
  }
}

absl::Nonnull<void*> ReferenceCountingMemoryManager::Allocate(
    size_t size, size_t alignment) {
  ABSL_DCHECK(absl::has_single_bit(alignment))
      << "alignment must be a power of 2: " << alignment;
  if (size == 0) {
    return nullptr;
  }
  if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
    return ::operator new(size);
  }
  return ::operator new(size, static_cast<std::align_val_t>(alignment));
}

bool ReferenceCountingMemoryManager::Deallocate(void* ptr, size_t size,
                                                size_t alignment) noexcept {
  ABSL_DCHECK(absl::has_single_bit(alignment))
      << "alignment must be a power of 2: " << alignment;
  if (ptr == nullptr) {
    ABSL_DCHECK_EQ(size, 0);
    return false;
  }
  ABSL_DCHECK_GT(size, 0);
  if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
    ::operator delete(ptr, size);
#else
    ::operator delete(ptr);
#endif
  } else {
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
    ::operator delete(ptr, size, static_cast<std::align_val_t>(alignment));
#else
    ::operator delete(ptr, static_cast<std::align_val_t>(alignment));
#endif
  }
  return true;
}

absl::Nonnull<std::unique_ptr<PoolingMemoryManager>>
NewThreadCompatiblePoolingMemoryManager() {
  return std::make_unique<ThreadCompatiblePoolingMemoryManager>();
}

absl::Nonnull<PoolingMemoryManager*>
MemoryManager::UnreachablePooling() noexcept {
  static absl::NoDestructor<UnreachablePoolingMemoryManager> instance;
  return &*instance;
}

MemoryManagerRef MemoryManagerRef::Unmanaged() {
  static UnmanagedPoolingMemoryManager instance;
  return MemoryManagerRef::Pooling(UnmanagedMemoryManagerVirtualTable(),
                                   instance);
}

}  // namespace cel
