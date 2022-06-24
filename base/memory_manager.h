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
#include "base/internal/data.h"
#include "base/internal/memory_manager.h"
#include "base/managed_memory.h"

namespace cel {

class MemoryManager;
class GlobalMemoryManager;
class ArenaMemoryManager;

// `MemoryManager` is an abstraction over memory management that supports
// different allocation strategies.
class MemoryManager {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static MemoryManager& Global();

  virtual ~MemoryManager() = default;

  // Allocates and constructs `T`. In the event of an allocation failure nullptr
  // is returned.
  template <typename T, typename... Args>
  std::enable_if_t<std::is_base_of_v<base_internal::Data, T>, ManagedMemory<T>>
  New(Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND ABSL_MUST_USE_RESULT {
    static_assert(std::is_base_of_v<base_internal::HeapData, T>,
                  "T must only be stored inline");
    if (!allocation_only_) {
      T* pointer = new T(std::forward<Args>(args)...);
      base_internal::Metadata::SetReferenceCounted(*pointer);
      return ManagedMemory<T>(pointer);
    }
    void* pointer = Allocate(sizeof(T), alignof(T));
    ::new (pointer) T(std::forward<Args>(args)...);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      OwnDestructor(pointer,
                    &base_internal::MemoryManagerDestructor<T>::Destruct);
    }
    base_internal::Metadata::SetArenaAllocated(*reinterpret_cast<T*>(pointer));
    return ManagedMemory<T>(reinterpret_cast<T*>(pointer));
  }

  // Allocates and constructs `T`. In the event of an allocation failure nullptr
  // is returned.
  template <typename T, typename... Args>
  std::enable_if_t<std::negation_v<std::is_base_of<base_internal::Data, T>>,
                   ManagedMemory<T>>
  New(Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND ABSL_MUST_USE_RESULT {
    if (!allocation_only_) {
      base_internal::ManagedMemoryDestructor destructor = nullptr;
      if constexpr (!std::is_trivially_destructible_v<T>) {
        destructor = &base_internal::MemoryManagerDestructor<T>::Destruct;
      }
      auto [state, pointer] = base_internal::ManagedMemoryState::New(
          sizeof(T), alignof(T), destructor);
      ::new (pointer) T(std::forward<Args>(args)...);
      return ManagedMemory<T>(reinterpret_cast<T*>(pointer), state);
    }
    void* pointer = Allocate(sizeof(T), alignof(T));
    ::new (pointer) T(std::forward<Args>(args)...);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      OwnDestructor(pointer,
                    &base_internal::MemoryManagerDestructor<T>::Destruct);
    }
    return ManagedMemory<T>(reinterpret_cast<T*>(pointer), nullptr);
  }

 private:
  friend class GlobalMemoryManager;
  friend class ArenaMemoryManager;

  // Only for use by GlobalMemoryManager and ArenaMemoryManager.
  explicit MemoryManager(bool allocation_only)
      : allocation_only_(allocation_only) {}

  // These are virtual private, ensuring only `MemoryManager` calls these.

  // Allocates memory of at least size `size` in bytes that is at least as
  // aligned as `align`.
  virtual void* Allocate(size_t size, size_t align) = 0;

  // Registers a destructor to be run upon destruction of the memory management
  // implementation.
  virtual void OwnDestructor(void* pointer, void (*destruct)(void*)) = 0;

  const bool allocation_only_;
};

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
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_MEMORY_MANAGER_H_
