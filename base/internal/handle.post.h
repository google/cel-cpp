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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_HANDLE_POST_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_HANDLE_POST_H_

#include <type_traits>
#include <utility>

#include "absl/base/optimization.h"
#include "base/memory_manager.h"

namespace cel::base_internal {

template <typename T>
struct HandleFactory<HandleType::kTransient, T> {
  template <typename F, typename... Args>
  static Transient<T> MakeInlined(Args&&... args) {
    static_assert(std::is_base_of_v<T, F>, "F is not derived from T");
    return Transient<T>(kHandleInPlace, kInlinedResource<F>,
                        std::forward<Args>(args)...);
  }

  template <typename F>
  static Transient<T> MakeUnmanaged(F& from) {
    static_assert(std::is_base_of_v<T, F>, "F is not derived from T");
    return Transient<T>(kHandleInPlace, kUnmanagedResource<F>, from);
  }
};

template <typename T>
struct HandleFactory<HandleType::kPersistent, T> {
  // Constructs a persistent handle whose underlying object is stored in the
  // handle itself.
  template <typename F, typename... Args>
  static std::enable_if_t<std::is_base_of_v<ResourceInlined, F>, Persistent<T>>
  Make(Args&&... args) {
    static_assert(std::is_base_of_v<Resource, F>,
                  "T is not derived from Resource");
    static_assert(std::is_base_of_v<T, F>, "F is not derived from T");
    return Persistent<T>(kHandleInPlace, kInlinedResource<F>,
                         std::forward<Args>(args)...);
  }

  // Constructs a persistent handle whose underlying object is heap allocated
  // and potentially reference counted, depending on the memory manager
  // implementation.
  template <typename F, typename... Args>
  static std::enable_if_t<std::negation_v<std::is_base_of<ResourceInlined, F>>,
                          Persistent<T>>
  Make(MemoryManager& memory_manager, Args&&... args) {
    static_assert(std::is_base_of_v<Resource, F>,
                  "T is not derived from Resource");
    static_assert(std::is_base_of_v<T, F>, "F is not derived from T");
#if defined(__cpp_lib_is_pointer_interconvertible) && \
    __cpp_lib_is_pointer_interconvertible >= 201907L
    // Only available in C++20.
    static_assert(std::is_pointer_interconvertible_base_of_v<Resource, F>,
                  "F must be pointer interconvertible to Resource");
#endif
    auto managed_memory = memory_manager.New<F>(std::forward<Args>(args)...);
    if (ABSL_PREDICT_FALSE(managed_memory == nullptr)) {
      return Persistent<T>();
    }
    bool unmanaged = GetManagedMemorySize(managed_memory) == 0;
#ifndef NDEBUG
    if (!unmanaged) {
      // Ensure there is no funny business going on by asserting that the size
      // and alignment are the same as F.
      ABSL_ASSERT(GetManagedMemorySize(managed_memory) == sizeof(F));
      ABSL_ASSERT(GetManagedMemoryAlignment(managed_memory) == alignof(F));
      // Ensure that the implementation F has correctly overriden
      // SizeAndAlignment().
      auto [size, align] = static_cast<const Resource*>(managed_memory.get())
                               ->SizeAndAlignment();
      ABSL_ASSERT(size == sizeof(F));
      ABSL_ASSERT(align == alignof(F));
      // Ensures that casting F to the most base class does not require
      // thunking, which occurs when using multiple inheritance. If thunking is
      // used our usage of memory manager will break. If you think you need
      // thunking, please consult the CEL team.
      ABSL_ASSERT(static_cast<const void*>(
                      static_cast<const Resource*>(managed_memory.get())) ==
                  static_cast<const void*>(managed_memory.get()));
    }
#endif
    // Convert ManagedMemory<T> to Persistent<T>, transferring reference
    // counting responsibility to it when applicable. `unmanaged` is true when
    // no reference counting is required.
    return unmanaged ? Persistent<T>(kHandleInPlace, kUnmanagedResource<F>,
                                     *ManagedMemoryRelease(managed_memory))
                     : Persistent<T>(kHandleInPlace, kManagedResource<F>,
                                     *ManagedMemoryRelease(managed_memory));
  }
};

template <typename T>
bool IsManagedHandle(const Transient<T>& handle) {
  return handle.impl_.IsManaged();
}

template <typename T>
bool IsUnmanagedHandle(const Transient<T>& handle) {
  return handle.impl_.IsUnmanaged();
}

template <typename T>
bool IsInlinedHandle(const Transient<T>& handle) {
  return handle.impl_.IsInlined();
}

template <typename T>
bool IsManagedHandle(const Persistent<T>& handle) {
  return handle.impl_.IsManaged();
}

template <typename T>
bool IsUnmanagedHandle(const Persistent<T>& handle) {
  return handle.impl_.IsUnmanaged();
}

template <typename T>
bool IsInlinedHandle(const Persistent<T>& handle) {
  return handle.impl_.IsInlined();
}

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_HANDLE_POST_H_
