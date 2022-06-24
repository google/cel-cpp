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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_ASSUME_ALIGNED_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_ASSUME_ALIGNED_H_

#include <memory>  // std::assume_aligned in C++20

#include "absl/base/attributes.h"
#include "absl/base/config.h"

namespace cel::internal {

// C++14 version of C++20's std::assume_aligned().
template <size_t N, typename T>
ABSL_MUST_USE_RESULT inline T* assume_aligned(T* pointer) noexcept {
#if defined(__cpp_lib_assume_aligned) && __cpp_lib_assume_aligned >= 201811L
  return std::assume_aligned<N>(pointer);
#elif (defined(__GNUC__) && !defined(__clang__)) || \
    ABSL_HAVE_BUILTIN(__builtin_assume_aligned)
  return static_cast<T*>(__builtin_assume_aligned(pointer, N));
#else
  return pointer;
#endif
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_ASSUME_ALIGNED_H_
