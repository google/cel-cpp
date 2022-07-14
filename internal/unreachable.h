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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_UNREACHABLE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_UNREACHABLE_H_

#include <cstdlib>
#include <utility>  // std::unreachable in C++20

#include "absl/base/attributes.h"
#include "absl/base/config.h"

namespace cel::internal {

// C++14 version of C++20's std::unreachable().
ABSL_ATTRIBUTE_NORETURN inline void unreachable() noexcept {
#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
  std::unreachable();
#elif defined(__GNUC__) || ABSL_HAVE_BUILTIN(__builtin_unreachable)
  __builtin_unreachable();
#elif defined(_MSC_VER)
  __assume(false);
#else
  std::abort();
#endif
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_UNREACHABLE_H_
