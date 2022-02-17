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
#include "base/internal/memory_manager.h"

namespace cel {

// `ManagedMemory` is a smart pointer which ensures any applicable object
// destructors and deallocation are eventually performed upon its destruction.
// While `ManagedManager` is derived from `std::unique_ptr`, it does not make
// any guarantees that destructors and deallocation are run immediately upon its
// destruction, just that they will eventually be performed.
template <typename T>
using ManagedMemory =
    std::unique_ptr<T, base_internal::MemoryManagerDeleter<T>>;

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
    auto [pointer, owned] = Allocate(size, align);
    if (ABSL_PREDICT_FALSE(pointer == nullptr)) {
      return ManagedMemory<T>();
    }
    ::new (pointer) T(std::forward<Args>(args)...);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      if (!owned) {
        OwnDestructor(pointer,
                      &base_internal::MemoryManagerDestructor<T>::Destruct);
      }
    }
    return ManagedMemory<T>(reinterpret_cast<T*>(pointer),
                            base_internal::MemoryManagerDeleter<T>(
                                owned ? this : nullptr, size, align));
  }

 protected:
  template <typename Pointer>
  struct AllocationResult final {
    Pointer pointer = nullptr;
    // If true, the responsibility of deallocating and destructing `pointer` is
    // passed to the caller of `Allocate`.
    bool owned = false;
  };

 private:
  template <typename T>
  friend class base_internal::MemoryManagerDeleter;

  // Delete a previous `New()` result when `AllocationResult::owned` is true.
  template <typename T>
  void Delete(T* pointer, size_t size, size_t align) {
    if (pointer != nullptr) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        pointer->~T();
      }
      Deallocate(pointer, size, align);
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
};

// Base class for all arena-based memory managers.
class ArenaMemoryManager : public MemoryManager {
 private:
  // Default implementation calls std::abort(). If you have a special case where
  // you support deallocating individual allocations, override this.
  void Deallocate(void* pointer, size_t size, size_t align) override;

  // OwnDestructor is typically required for arena-based memory managers.
  void OwnDestructor(void* pointer, void (*destruct)(void*)) override = 0;
};

namespace base_internal {

template <typename T>
void MemoryManagerDeleter<T>::operator()(T* pointer) const {
  if (memory_manager_) {
    memory_manager_->Delete(const_cast<std::remove_const_t<T>*>(pointer), size_,
                            align_);
  }
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_MEMORY_MANAGER_H_
