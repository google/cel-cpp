#include "base/memory_manager.h"

// Copyright 2022 Google LLC
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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/numeric/bits.h"
#include "internal/no_destructor.h"

namespace cel {

namespace {

class GlobalMemoryManager final : public MemoryManager {
 public:
  GlobalMemoryManager() : MemoryManager() {}

 private:
  AllocationResult<void*> Allocate(size_t size, size_t align) override {
    void* pointer;
    if (ABSL_PREDICT_TRUE(align <= alignof(std::max_align_t))) {
      pointer = ::operator new(size, std::nothrow);
    } else {
      pointer = ::operator new(size, static_cast<std::align_val_t>(align),
                               std::nothrow);
    }
    return {pointer};
  }

  void Deallocate(void* pointer, size_t size, size_t align) override {
    if (ABSL_PREDICT_TRUE(align <= alignof(std::max_align_t))) {
      ::operator delete(pointer, size);
    } else {
      ::operator delete(pointer, size, static_cast<std::align_val_t>(align));
    }
  }
};

struct ControlBlock final {
  constexpr explicit ControlBlock(MemoryManager* memory_manager)
      : refs(1), memory_manager(memory_manager) {}

  ControlBlock(const ControlBlock&) = delete;
  ControlBlock(ControlBlock&&) = delete;
  ControlBlock& operator=(const ControlBlock&) = delete;
  ControlBlock& operator=(ControlBlock&&) = delete;

  mutable std::atomic<intptr_t> refs;
  MemoryManager* memory_manager;

  void Ref() const {
    const auto cnt = refs.fetch_add(1, std::memory_order_relaxed);
    ABSL_ASSERT(cnt >= 1);
  }

  bool Unref() const {
    const auto cnt = refs.fetch_sub(1, std::memory_order_acq_rel);
    ABSL_ASSERT(cnt >= 1);
    return cnt == 1;
  }
};

size_t AlignUp(size_t size, size_t align) {
  ABSL_ASSERT(size != 0);
  ABSL_ASSERT(absl::has_single_bit(align));  // Assert aligned to power of 2.
#if ABSL_HAVE_BUILTIN(__builtin_align_up)
  return __builtin_align_up(size, align);
#else
  return (size + align - size_t{1}) & ~(align - size_t{1});
#endif
}

inline constexpr size_t kControlBlockSize = sizeof(ControlBlock);
inline constexpr size_t kControlBlockAlign = alignof(ControlBlock);

// When not using arena-based allocation, MemoryManager needs to embed a pointer
// to itself in the allocation block so the same memory manager can be used to
// deallocate. When the alignment requested is less than or equal to that of the
// native pointer alignment it is embedded at the beginning of the allocated
// block, otherwise its at the end.
//
// For allocations requiring alignment greater than alignof(ControlBlock) we
// cannot place the control block in front as it would change the alignment of
// T, resulting in undefined behavior. For allocations requiring less alignment
// than alignof(ControlBlock), we should not place the control back in back as
// it would waste memory due to having to pad the allocation to ensure
// ControlBlock itself is aligned.
enum class Placement {
  kBefore = 0,
  kAfter,
};

constexpr Placement GetPlacement(size_t align) {
  return ABSL_PREDICT_TRUE(align <= kControlBlockAlign) ? Placement::kBefore
                                                        : Placement::kAfter;
}

void* AdjustAfterAllocation(MemoryManager* memory_manager, void* pointer,
                            size_t size, size_t align) {
  switch (GetPlacement(align)) {
    case Placement::kBefore:
      // Store the pointer to the memory manager at the beginning of the
      // allocated block and adjust the pointer to immediately after it.
      ::new (pointer) ControlBlock(memory_manager);
      pointer = static_cast<void*>(static_cast<uint8_t*>(pointer) +
                                   kControlBlockSize);
      break;
    case Placement::kAfter:
      // Store the pointer to the memory manager at the end of the allocated
      // block. Don't need to adjust the pointer.
      ::new (static_cast<void*>(static_cast<uint8_t*>(pointer) + size -
                                kControlBlockSize))
          ControlBlock(memory_manager);
      break;
  }
  return pointer;
}

void* AdjustForDeallocation(void* pointer, size_t align) {
  switch (GetPlacement(align)) {
    case Placement::kBefore:
      // We need to back up kPointerSize as that is actually the original
      // allocated address returned from `Allocate`.
      pointer = static_cast<void*>(static_cast<uint8_t*>(pointer) -
                                   kControlBlockSize);
      break;
    case Placement::kAfter:
      // No need to do anything.
      break;
  }
  return pointer;
}

ControlBlock* GetControlBlock(const void* pointer, size_t size, size_t align) {
  ControlBlock* control_block;
  switch (GetPlacement(align)) {
    case Placement::kBefore:
      // Embedded reference count block is located just before `pointer`.
      control_block = reinterpret_cast<ControlBlock*>(
          static_cast<uint8_t*>(const_cast<void*>(pointer)) -
          kControlBlockSize);
      break;
    case Placement::kAfter:
      // Embedded reference count block is located at `pointer + size -
      // kControlBlockSize`.
      control_block = reinterpret_cast<ControlBlock*>(
          static_cast<uint8_t*>(const_cast<void*>(pointer)) + size -
          kControlBlockSize);
      break;
  }
  return control_block;
}

size_t AdjustAllocationSize(size_t size, size_t align) {
  if (GetPlacement(align) == Placement::kAfter) {
    size = AlignUp(size, kControlBlockAlign);
  }
  return size + kControlBlockSize;
}

}  // namespace

MemoryManager& MemoryManager::Global() {
  static internal::NoDestructor<GlobalMemoryManager> instance;
  return *instance;
}

void* MemoryManager::AllocateInternal(size_t& size, size_t& align) {
  ABSL_ASSERT(size != 0);
  ABSL_ASSERT(absl::has_single_bit(align));  // Assert aligned to power of 2.
  size_t adjusted_size = size;
  if (!allocation_only_) {
    adjusted_size = AdjustAllocationSize(adjusted_size, align);
  }
  auto [pointer] = Allocate(adjusted_size, align);
  if (ABSL_PREDICT_TRUE(pointer != nullptr) && !allocation_only_) {
    pointer = AdjustAfterAllocation(this, pointer, adjusted_size, align);
  } else {
    // 0 is not a valid result of sizeof. So we use that to signal to the
    // deleter that it should not perform a deletion and that the memory manager
    // will.
    size = align = 0;
  }
  return pointer;
}

void MemoryManager::DeallocateInternal(void* pointer, size_t size,
                                       size_t align) {
  ABSL_ASSERT(pointer != nullptr);
  ABSL_ASSERT(size != 0);
  ABSL_ASSERT(absl::has_single_bit(align));  // Assert aligned to power of 2.
  // `size` is the unadjusted size, the original sizeof(T) used during
  // allocation. We need to adjust it to match the allocation size.
  size = AdjustAllocationSize(size, align);
  ControlBlock* control_block = GetControlBlock(pointer, size, align);
  MemoryManager* memory_manager = control_block->memory_manager;
  if constexpr (!std::is_trivially_destructible_v<ControlBlock>) {
    control_block->~ControlBlock();
  }
  pointer = AdjustForDeallocation(pointer, align);
  memory_manager->Deallocate(pointer, size, align);
}

void MemoryManager::Ref(const void* pointer, size_t size, size_t align) {
  if (pointer != nullptr && size != 0) {
    ABSL_ASSERT(absl::has_single_bit(align));  // Assert aligned to power of 2.
    // `size` is the unadjusted size, the original sizeof(T) used during
    // allocation. We need to adjust it to match the allocation size.
    size = AdjustAllocationSize(size, align);
    GetControlBlock(pointer, size, align)->Ref();
  }
}

bool MemoryManager::UnrefInternal(const void* pointer, size_t size,
                                  size_t align) {
  bool cleanup = false;
  if (pointer != nullptr && size != 0) {
    ABSL_ASSERT(absl::has_single_bit(align));  // Assert aligned to power of 2.
    // `size` is the unadjusted size, the original sizeof(T) used during
    // allocation. We need to adjust it to match the allocation size.
    size = AdjustAllocationSize(size, align);
    cleanup = GetControlBlock(pointer, size, align)->Unref();
  }
  return cleanup;
}

void MemoryManager::OwnDestructor(void* pointer, void (*destruct)(void*)) {
  static_cast<void>(pointer);
  static_cast<void>(destruct);
  // OwnDestructor is only called for arena-based memory managers by `New`. If
  // we got here, something is seriously wrong so crashing is okay.
  std::abort();
}

void ArenaMemoryManager::Deallocate(void* pointer, size_t size, size_t align) {
  static_cast<void>(pointer);
  static_cast<void>(size);
  static_cast<void>(align);
  // Most arena-based allocators will not deallocate individual allocations, so
  // we default the implementation to std::abort().
  std::abort();
}

}  // namespace cel
