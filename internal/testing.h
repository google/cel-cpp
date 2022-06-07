// Copyright 2021 Google LLC
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

// Implements a gMock matcher that checks that an asylo::StaturOr<T> or
// absl::StatusOr<T> has an OK status and that the contained T value matches
// another matcher.
template <typename StatusOrT>
class IsOkAndHoldsMatcher
    : public ::testing::MatcherInterface<const StatusOrT &> {
  using ValueType = typename StatusOrT::value_type;

 public:
  template <typename MatcherT>
  explicit IsOkAndHoldsMatcher(MatcherT &&value_matcher)
      : value_matcher_(
            ::testing::SafeMatcherCast<const ValueType &>(value_matcher)) {}

  // From testing::MatcherInterface.
  void DescribeTo(std::ostream *os) const override {
    *os << "is OK and contains a value that ";
    value_matcher_.DescribeTo(os);
  }

  // From testing::MatcherInterface.
  void DescribeNegationTo(std::ostream *os) const override {
    *os << "is not OK or contains a value that ";
    value_matcher_.DescribeNegationTo(os);
  }

  // From testing::MatcherInterface.
  bool MatchAndExplain(
      const StatusOrT &status_or,
      ::testing::MatchResultListener *listener) const override {
    if (!status_or.ok()) {
      *listener << "which is not OK";
      return false;
    }

    ::testing::StringMatchResultListener value_listener;
    bool is_a_match =
        value_matcher_.MatchAndExplain(*status_or, &value_listener);
    std::string value_explanation = value_listener.str();
    if (!value_explanation.empty()) {
      *listener << absl::StrCat("which contains a value ", value_explanation);
    }

    return is_a_match;
  }

 private:
  const ::testing::Matcher<const ValueType &> value_matcher_;
};

// A polymorphic IsOkAndHolds() matcher.
//
// IsOkAndHolds() returns a matcher that can be used to process an IsOkAndHolds
// expectation. However, the value type T is not provided when IsOkAndHolds() is
// invoked. The value type is only inferable when the gtest framework invokes
// the matcher with a value. Consequently, the IsOkAndHolds() function must
// return an object that is implicitly convertible to a matcher for StatusOr<T>.
// gtest refers to such an object as a polymorphic matcher, since it can be used
// to match with more than one type of value.
template <typename ValueMatcherT>
class IsOkAndHoldsGenerator {
 public:
  explicit IsOkAndHoldsGenerator(ValueMatcherT value_matcher)
      : value_matcher_(std::move(value_matcher)) {}

  template <typename T>
  operator ::testing::Matcher<const absl::StatusOr<T> &>() const {
    return ::testing::MakeMatcher(
        new IsOkAndHoldsMatcher<absl::StatusOr<T>>(value_matcher_));
  }

 private:
  const ValueMatcherT value_matcher_;
};

template <typename ValueMatcherT>
IsOkAndHoldsGenerator<ValueMatcherT> IsOkAndHolds(
    ValueMatcherT value_matcher) {
  return IsOkAndHoldsGenerator<ValueMatcherT>(value_matcher);
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_H_
