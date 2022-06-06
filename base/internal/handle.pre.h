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

// IWYU pragma: private, include "base/handle.h"

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_HANDLE_PRE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_HANDLE_PRE_H_

#include <cstddef>
#include <type_traits>
#include <utility>

#include "base/memory_manager.h"

namespace cel {

class Type;
class Value;

template <typename T>
class Persistent;

class MemoryManager;

namespace base_internal {

class TypeHandleBase;
class ValueHandleBase;

// Enumeration of different types of handles.
enum class HandleType {
  kPersistent = 0,
};

template <HandleType H, typename T, typename = void>
struct HandleTraits;

// Convenient aliases.
template <typename T>
using PersistentHandleTraits = HandleTraits<HandleType::kPersistent, T>;

template <HandleType H, typename T>
struct HandleFactory;

// Convenient aliases.
template <typename T>
using PersistentHandleFactory = HandleFactory<HandleType::kPersistent, T>;

struct HandleInPlace {
  explicit HandleInPlace() = default;
};

// Disambiguation tag used to select the appropriate constructor on Persistent
// and Transient. Think std::in_place.
inline constexpr HandleInPlace kHandleInPlace{};

// If IsManagedHandle returns true, get a reference to the memory manager that
// is managing it.
template <typename T>
MemoryManager& GetMemoryManager(const Persistent<T>& handle);

// Virtual base class for all classes that can be managed by handles.
class Resource {
 public:
  virtual ~Resource() = default;

  Resource& operator=(const Resource&) = delete;
  Resource& operator=(Resource&&) = delete;

 private:
  friend class cel::Type;
  friend class cel::Value;
  friend class TypeHandleBase;
  friend class ValueHandleBase;
  template <HandleType H, typename T>
  friend struct HandleFactory;
  template <typename T>
  friend MemoryManager& GetMemoryManager(const Persistent<T>& handle);

  Resource() = default;
  Resource(const Resource&) = default;
  Resource(Resource&&) = default;

  // For non-inlined resources that are reference counted, this is the result of
  // `sizeof` and `alignof` for the most derived class.
  virtual std::pair<size_t, size_t> SizeAndAlignment() const = 0;

  // Called by TypeHandleBase, ValueHandleBase, Type, and Value for reference
  // counting.
  void Ref() const {
    auto [size, align] = SizeAndAlignment();
    MemoryManager::Ref(this, size, align);
  }

  // Called by TypeHandleBase, ValueHandleBase, Type, and Value for reference
  // counting.
  void Unref() const {
    auto [size, align] = SizeAndAlignment();
    MemoryManager::Unref(this, size, align);
  }
};

// Non-virtual base class for all classes that can be stored inline in handles.
// This is primarily used with SFINAE.
class ResourceInlined {};

template <typename T>
struct InlinedResource {
  explicit InlinedResource() = default;
};

// Disambiguation tag used to select the appropriate constructor in the handle
// implementation. Think std::in_place.
template <typename T>
inline constexpr InlinedResource<T> kInlinedResource{};

template <typename T>
struct ManagedResource {
  explicit ManagedResource() = default;
};

// Disambiguation tag used to select the appropriate constructor in the handle
// implementation. Think std::in_place.
template <typename T>
inline constexpr ManagedResource<T> kManagedResource{};

template <typename T>
struct UnmanagedResource {
  explicit UnmanagedResource() = default;
};

// Disambiguation tag used to select the appropriate constructor in the handle
// implementation. Think std::in_place.
template <typename T>
inline constexpr UnmanagedResource<T> kUnmanagedResource{};

// Non-virtual base class enforces type requirements via static_asserts for
// types used with handles.
template <typename T>
struct HandlePolicy {
  static_assert(!std::is_reference_v<T>, "Handles do not support references");
  static_assert(!std::is_pointer_v<T>, "Handles do not support pointers");
  static_assert(std::is_class_v<T>, "Handles only support classes");
  static_assert(!std::is_volatile_v<T>, "Handles do not support volatile");
  static_assert((std::is_base_of_v<Resource, std::remove_const_t<T>> &&
                 !std::is_same_v<Resource, std::remove_const_t<T>> &&
                 !std::is_same_v<ResourceInlined, std::remove_const_t<T>>),
                "Handles do not support this type");
};

template <typename T>
bool IsManagedHandle(const Persistent<T>& handle);
template <typename T>
bool IsUnmanagedHandle(const Persistent<T>& handle);
template <typename T>
bool IsInlinedHandle(const Persistent<T>& handle);

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_HANDLE_PRE_H_
