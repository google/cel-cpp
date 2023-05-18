// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TESTING_TYPE_MATCHERS_H_
#define THIRD_PARTY_CEL_CPP_BASE_TESTING_TYPE_MATCHERS_H_

#include <ostream>
#include <type_traits>

#include "base/handle.h"
#include "base/testing/handle_matchers.h"
#include "base/type.h"
#include "internal/testing.h"

namespace cel_testing {

namespace base_internal {

template <typename T>
class TypeIsImpl {
 public:
  constexpr TypeIsImpl() = default;

  template <typename U>
  operator testing::Matcher<U>() const {  // NOLINT(google-explicit-constructor)
    return testing::Matcher<U>(new Impl<const U&>());
  }

 private:
  template <typename U>
  class Impl final : public testing::MatcherInterface<U> {
   public:
    Impl() = default;

    void DescribeTo(std::ostream* os) const override {
      *os << "instance of type " << testing::internal::GetTypeName<T>();
    }

    void DescribeNegationTo(std::ostream* os) const override {
      *os << "not instance of type " << testing::internal::GetTypeName<T>();
    }

    bool MatchAndExplain(
        U u, testing::MatchResultListener* listener) const override {
      if (!IndirectImpl(u).template Is<T>()) {
        return false;
      }
      *listener << "which is an instance of type "
                << testing::internal::GetTypeName<T>();
      return true;
    }
  };
};

}  // namespace base_internal

template <typename T>
base_internal::TypeIsImpl<T> TypeIs() {
  static_assert(std::is_base_of_v<cel::Type, T>);
  return base_internal::TypeIsImpl<T>();
}

}  // namespace cel_testing

#endif  // THIRD_PARTY_CEL_CPP_BASE_TESTING_TYPE_MATCHERS_H_
