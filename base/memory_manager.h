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

#ifndef THIRD_PARTY_CEL_CPP_BASE_MEMORY_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_BASE_MEMORY_MANAGER_H_

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "base/internal/memory_manager.pre.h"  // IWYU pragma: export

namespace cel {

class MemoryManager;
class ArenaMemoryManager;

// `ManagedMemory` is a smart pointer which ensures any applicable object
// destructors and deallocation are eventually performed. Copying does not
// actually copy the underlying T, instead a pointer is copied and optionally
// reference counted. Moving does not actually move the underlying T, instead a
// pointer is moved.
//
// TODO(issues/5): consider feature parity with std::unique_ptr<T>
template <typename T>
class ManagedMemory final {
 public:
  ManagedMemory() = default;

  ManagedMemory(const ManagedMemory<T>& other)
      : ptr_(other.ptr_), size_(other.size_), align_(other.align_) {
    Ref();
  }

  ManagedMemory(ManagedMemory<T>&& other)
      : ptr_(other.ptr_), size_(other.size_), align_(other.align_) {
    other.ptr_ = nullptr;
    other.size_ = other.align_ = 0;
  }

  ~ManagedMemory() { Unref(); }

  ManagedMemory<T>& operator=(const ManagedMemory<T>& other) {
    if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
      other.Ref();
      Unref();
      ptr_ = other.ptr_;
      size_ = other.size_;
      align_ = other.align_;
    }
    return *this;
  }

  ManagedMemory<T>& operator=(ManagedMemory<T>&& other) {
    if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
      reset();
      swap(other);
    }
    return *this;
  }

  T* release() {
    ABSL_ASSERT(size_ == 0);
    return base_internal::ManagedMemoryRelease(*this);
  }

  void reset() {
    Unref();
    ptr_ = nullptr;
    size_ = align_ = 0;
  }

  void swap(ManagedMemory<T>& other) {
    std::swap(ptr_, other.ptr_);
    std::swap(size_, other.size_);
    std::swap(align_, other.align_);
  }

  constexpr T* get() const { return ptr_; }

  constexpr T& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(static_cast<bool>(*this));
    return *get();
  }

  constexpr T* operator->() const {
    ABSL_ASSERT(static_cast<bool>(*this));
    return get();
  }

  constexpr explicit operator bool() const { return get() != nullptr; }

 private:
  friend class MemoryManager;
  template <typename F>
  friend constexpr size_t base_internal::GetManagedMemorySize(
      const ManagedMemory<F>& managed_memory);
  template <typename F>
  friend constexpr size_t base_internal::GetManagedMemoryAlignment(
      const ManagedMemory<F>& managed_memory);
  template <typename F>
  friend constexpr F* base_internal::ManagedMemoryRelease(
      ManagedMemory<F>& managed_memory);

  constexpr ManagedMemory(T* ptr, size_t size, size_t align)
      : ptr_(ptr), size_(size), align_(align) {}

  void Ref() const;

  void Unref() const;

  T* ptr_ = nullptr;
  size_t size_ = 0;
  size_t align_ = 0;
};

template <typename T>
constexpr bool operator==(const ManagedMemory<T>& lhs, std::nullptr_t) {
  return !static_cast<bool>(lhs);
}

template <typename T>
constexpr bool operator==(std::nullptr_t, const ManagedMemory<T>& rhs) {
  return !static_cast<bool>(rhs);
}

template <typename T>
constexpr bool operator!=(const ManagedMemory<T>& lhs, std::nullptr_t) {
  return !operator==(lhs, nullptr);
}

template <typename T>
constexpr bool operator!=(std::nullptr_t, const ManagedMemory<T>& rhs) {
  return !operator==(nullptr, rhs);
}

// `MemoryManager` is an abstraction over memory management that supports
// different allocation strategies.
class MemoryManager {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static MemoryManager& Global();

  virtual ~MemoryManager() = default;

  // Allocates and constructs `T`. In the event of an allocation failure nullptr
  // is returned.
  template <typename T, typename... Args>
  ManagedMemory<T> New(Args&&... args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND ABSL_MUST_USE_RESULT {
    size_t size = sizeof(T);
    size_t align = alignof(T);
    void* pointer = AllocateInternal(size, align);
    if (ABSL_PREDICT_TRUE(pointer != nullptr)) {
      ::new (pointer) T(std::forward<Args>(args)...);
      if constexpr (!std::is_trivially_destructible_v<T>) {
        if (allocation_only_) {
          OwnDestructor(pointer,
                        &base_internal::MemoryManagerDestructor<T>::Destruct);
        }
      }
    }
    return ManagedMemory<T>(reinterpret_cast<T*>(pointer), size, align);
  }

 protected:
  MemoryManager() : MemoryManager(false) {}

  template <typename Pointer>
  struct AllocationResult final {
    Pointer pointer = nullptr;
  };

 private:
  template <typename T>
  friend class ManagedMemory;
  friend class ArenaMemoryManager;
  friend class base_internal::Resource;
  friend MemoryManager& base_internal::GetMemoryManager(const void* pointer,
                                                        size_t size,
                                                        size_t align);

  // Only for use by ArenaMemoryManager.
  explicit MemoryManager(bool allocation_only)
      : allocation_only_(allocation_only) {}

  static MemoryManager& Get(const void* pointer, size_t size, size_t align);

  void* AllocateInternal(size_t& size, size_t& align);

  static void DeallocateInternal(void* pointer, size_t size, size_t align);

  // Potentially increment the reference count in the control block for the
  // previously allocated memory from `New()`. This is intended to be called
  // from `ManagedMemory<T>`.
  //
  // If size is 0, then the allocation was arena-based.
  static void Ref(const void* pointer, size_t size, size_t align);

  // Potentially decrement the reference count in the control block for the
  // previously allocated memory from `New()`. Returns true if `Delete()` should
  // be called.
  //
  // If size is 0, then the allocation was arena-based and this call is a noop.
  static bool UnrefInternal(const void* pointer, size_t size, size_t align);

  // Delete a previous `New()` result when `allocation_only_` is false.
  template <typename T>
  static void Delete(T* pointer, size_t size, size_t align) {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      pointer->~T();
    }
    DeallocateInternal(
        static_cast<void*>(const_cast<std::remove_const_t<T>*>(pointer)), size,
        align);
  }

  // Potentially decrement the reference count in the control block and
  // deallocate the memory for the previously allocated memory from `New()`.
  // This is intended to be called from `ManagedMemory<T>`.
  //
  // If size is 0, then the allocation was arena-based and this call is a noop.
  template <typename T>
  static void Unref(T* pointer, size_t size, size_t align) {
    if (UnrefInternal(pointer, size, align)) {
      Delete(pointer, size, align);
    }
  }

  // These are virtual private, ensuring only `MemoryManager` calls these. Which
  // methods need to be implemented and which are called depends on whether the
  // implementation is using arena memory management or not.
  //
  // If the implementation is using arenas then `Deallocate()` will never be
  // called, `OwnDestructor` must be implemented, and `AllocationOnly` must
  // return true. If the implementation is *not* using arenas then `Deallocate`
  // must be implemented, `OwnDestructor` will never be called, and
  // `AllocationOnly` will return false.

  // Allocates memory of at least size `size` in bytes that is at least as
  // aligned as `align`.
  virtual AllocationResult<void*> Allocate(size_t size, size_t align) = 0;

  // Deallocate the given pointer previously allocated via `Allocate`, assuming
  // `AllocationResult::owned` was true. Calling this when
  // `AllocationResult::owned` was false is undefined behavior.
  virtual void Deallocate(void* pointer, size_t size, size_t align) = 0;

  // Registers a destructor to be run upon destruction of the memory management
  // implementation.
  //
  // This method is only valid for arena memory managers.
  virtual void OwnDestructor(void* pointer, void (*destruct)(void*));

  const bool allocation_only_;
};

template <typename T>
void ManagedMemory<T>::Ref() const {
  MemoryManager::Ref(ptr_, size_, align_);
}

template <typename T>
void ManagedMemory<T>::Unref() const {
  MemoryManager::Unref(ptr_, size_, align_);
}

namespace extensions {
class ProtoMemoryManager;
}

// Base class for all arena-based memory managers.
class ArenaMemoryManager : public MemoryManager {
 public:
  // Returns the default implementation of an arena-based memory manager. In
  // most cases it should be good enough, however you should not rely on its
  // performance characteristics.
  static std::unique_ptr<ArenaMemoryManager> Default();

 protected:
  ArenaMemoryManager() : ArenaMemoryManager(true) {}

 private:
  friend class extensions::ProtoMemoryManager;

  // Private so that only ProtoMemoryManager can use it for legacy reasons. All
  // other derivations of ArenaMemoryManager should be allocation-only.
  explicit ArenaMemoryManager(bool allocation_only)
      : MemoryManager(allocation_only) {}

  // Default implementation calls std::abort(). If you have a special case where
  // you support deallocating individual allocations, override this.
  void Deallocate(void* pointer, size_t size, size_t align) override;

  // OwnDestructor is typically required for arena-based memory managers.
  void OwnDestructor(void* pointer, void (*destruct)(void*)) override = 0;
};

}  // namespace cel

#include "base/internal/memory_manager.post.h"  // IWYU pragma: export

#endif  // THIRD_PARTY_CEL_CPP_BASE_MEMORY_MANAGER_H_
