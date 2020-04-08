/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_BASE_STATUS_MACROS_H_
#define THIRD_PARTY_CEL_CPP_BASE_STATUS_MACROS_H_

#include <assert.h>  // for use with down_cast<>

#include <type_traits>

// Early-returns the status if it is in error; otherwise, proceeds.
//
// The argument expression is guaranteed to be evaluated exactly once.
#if !defined(RETURN_IF_ERROR)
#define RETURN_IF_ERROR(__status) \
  do {                            \
    auto _status = __status;      \
    if (!_status.ok()) {          \
      return _status;             \
    }                             \
  } while (false)
#endif

template <typename To, typename From>  // use like this: down_cast<T*>(foo);
inline To down_cast(From* f) {         // so we only accept pointers
  static_assert(
      (std::is_base_of<From, typename std::remove_pointer<To>::type>::value),
      "target type not derived from source type");

  // We skip the assert and hence the dynamic_cast if RTTI is disabled.
#if !defined(__GNUC__) || defined(__GXX_RTTI)
  // Uses RTTI in dbg and fastbuild. asserts are disabled in opt builds.
  assert(f == nullptr || dynamic_cast<To>(f) != nullptr);
#endif  // !defined(__GNUC__) || defined(__GXX_RTTI)

  return static_cast<To>(f);
}

#if !defined(ASSERT_OK)
#define ASSERT_OK(expression) ASSERT_TRUE(expression.ok())
#endif

#if !defined(EXPECT_OK)
#define EXPECT_OK(expression) EXPECT_TRUE(expression.ok())
#endif

#endif  // THIRD_PARTY_CEL_CPP_BASE_STATUS_MACROS_H_
