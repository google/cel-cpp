/*
 * Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_H_

#include <ostream>
#include <string>
#include <type_traits>

#include "gmock/gmock.h"  // IWYU pragma: export
#include "gtest/gtest.h"  // IWYU pragma: export
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/status_builder.h"
#include "internal/status_macros.h"

#ifndef ASSERT_OK
#define ASSERT_OK(expr) ASSERT_THAT(expr, ::cel::internal::IsOk())
#endif

#ifndef EXPECT_OK
#define EXPECT_OK(expr) EXPECT_THAT(expr, ::cel::internal::IsOk())
#endif

#ifndef ASSERT_OK_AND_ASSIGN
#define ASSERT_OK_AND_ASSIGN(lhs, rhs) \
  CEL_ASSIGN_OR_RETURN(                \
      lhs, rhs, ::cel::internal::AddFatalFailure(__FILE__, __LINE__, #rhs, _))
#endif

namespace cel::internal {

inline const absl::Status& GetStatus(const absl::Status& status) {
  return status;
}

template <typename T>
inline const absl::Status& GetStatus(const absl::StatusOr<T>& status) {
  return status.status();
}

// StatusIs() is a polymorphic matcher.  This class is the common
// implementation of it shared by all types T where StatusIs() can be
// used as a Matcher<T>.
class StatusIsMatcherCommonImpl {
 public:
  StatusIsMatcherCommonImpl(
      ::testing::Matcher<absl::StatusCode> code_matcher,
      ::testing::Matcher<const std::string&> message_matcher)
      : code_matcher_(std::move(code_matcher)),
        message_matcher_(std::move(message_matcher)) {}

  void DescribeTo(std::ostream* os) const;

  void DescribeNegationTo(std::ostream* os) const;

  bool MatchAndExplain(const absl::Status& status,
                       ::testing::MatchResultListener* result_listener) const;

 private:
  const ::testing::Matcher<absl::StatusCode> code_matcher_;
  const ::testing::Matcher<const std::string&> message_matcher_;
};

// Monomorphic implementation of matcher StatusIs() for a given type
// T.  T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoStatusIsMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  explicit MonoStatusIsMatcherImpl(StatusIsMatcherCommonImpl common_impl)
      : common_impl_(std::move(common_impl)) {}

  void DescribeTo(std::ostream* os) const override {
    common_impl_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    common_impl_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      T actual_value,
      ::testing::MatchResultListener* result_listener) const override {
    return common_impl_.MatchAndExplain(GetStatus(actual_value),
                                        result_listener);
  }

 private:
  StatusIsMatcherCommonImpl common_impl_;
};

// Implements StatusIs() as a polymorphic matcher.
class StatusIsMatcher {
 public:
  StatusIsMatcher(::testing::Matcher<absl::StatusCode> code_matcher,
                  ::testing::Matcher<const std::string&> message_matcher)
      : common_impl_(std::move(code_matcher), std::move(message_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the given
  // type.  T can be StatusOr<>, Status, or a reference to either of them.
  template <typename T>
  operator ::testing::Matcher<T>() const {  // NOLINT
    return ::testing::MakeMatcher(new MonoStatusIsMatcherImpl<T>(common_impl_));
  }

 private:
  const StatusIsMatcherCommonImpl common_impl_;
};

// Monomorphic implementation of matcher IsOk() for a given type T.
// T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoIsOkMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  void DescribeTo(std::ostream* os) const override { *os << "is OK"; }
  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not OK";
  }
  bool MatchAndExplain(T actual_value,
                       ::testing::MatchResultListener*) const override {
    return GetStatus(actual_value).ok();
  }
};

// Implements IsOk() as a polymorphic matcher.
class IsOkMatcher {
 public:
  template <typename T>
  operator ::testing::Matcher<T>() const {  // NOLINT
    return ::testing::MakeMatcher(new MonoIsOkMatcherImpl<T>());
  }
};

// Returns a gMock matcher that matches a Status or StatusOr<> whose status code
// matches code_matcher, and whose error message matches message_matcher.
template <typename StatusCodeMatcher>
StatusIsMatcher StatusIs(
    StatusCodeMatcher&& code_matcher,
    ::testing::Matcher<const std::string&> message_matcher) {
  return StatusIsMatcher(std::forward<StatusCodeMatcher>(code_matcher),
                         std::move(message_matcher));
}

// Returns a gMock matcher that matches a Status or StatusOr<> whose status code
// matches code_matcher.
template <typename StatusCodeMatcher>
StatusIsMatcher StatusIs(StatusCodeMatcher&& code_matcher) {
  return StatusIs(std::forward<StatusCodeMatcher>(code_matcher), ::testing::_);
}

void AddFatalFailure(const char* file, int line, absl::string_view expression,
                     const StatusBuilder& builder);

// Returns a gMock matcher that matches a Status or StatusOr<> which is OK.
inline IsOkMatcher IsOk() { return IsOkMatcher(); }

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_H_
