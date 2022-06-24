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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_LAUNDER_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_LAUNDER_H_

#if __cplusplus >= 201703L
#include <new>
#endif

#include "absl/base/attributes.h"
#include "absl/base/config.h"

namespace cel::internal {

// C++14 version of C++17's std::launder().
template <typename T>
ABSL_MUST_USE_RESULT inline T* launder(T* pointer) noexcept {
#if __cplusplus >= 201703L
  return std::launder(pointer);
#elif ABSL_HAVE_BUILTIN(__builtin_launder) || \
    (defined(__GNUC__) && __GNUC__ >= 7)
  return __builtin_launder(pointer);
#else
  // Fallback to undefined behavior.
  return pointer;
#endif
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_LAUNDER_H_
