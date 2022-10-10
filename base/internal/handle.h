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

#include <type_traits>

#include "base/internal/data.h"

namespace cel::base_internal {

template <typename T, typename = void>
struct HandleTraits;

template <typename T>
struct HandleFactory;

// Non-virtual base class enforces type requirements via static_asserts for
// types used with handles.
template <typename T>
struct HandlePolicy {
  static_assert(!std::is_reference_v<T>, "Handles do not support references");
  static_assert(!std::is_pointer_v<T>, "Handles do not support pointers");
  static_assert(std::is_class_v<T>, "Handles only support classes");
  static_assert(!std::is_volatile_v<T>, "Handles do not support volatile");
  static_assert(!std::is_const_v<T>, "Handles do not support const");
  static_assert((std::is_base_of_v<Data, T> && !std::is_same_v<Data, T>),
                "Handles do not support this type");
};

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_HANDLE_PRE_H_
