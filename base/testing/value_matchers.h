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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TESTING_VALUE_MATCHERS_H_
#define THIRD_PARTY_CEL_CPP_BASE_TESTING_VALUE_MATCHERS_H_

#include <cstdint>
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/handle.h"
#include "base/testing/handle_matchers.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/double_value.h"
#include "base/values/duration_value.h"
#include "base/values/int_value.h"
#include "base/values/null_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/uint_value.h"
#include "internal/testing.h"

namespace cel_testing {

namespace base_internal {

template <typename T>
class ValueIsImpl {
 public:
  constexpr ValueIsImpl() = default;

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
      *listener << "which is an instance of value "
                << testing::internal::GetTypeName<T>();
      return true;
    }
  };
};

template <typename T, typename = void>
struct ValueOfTraits;

template <>
struct ValueOfTraits<cel::BoolValue, void> {
  static absl::StatusOr<cel::Handle<cel::BoolValue>> Create(
      cel::ValueFactory& value_factory, bool value) {
    return value_factory.CreateBoolValue(value);
  }

  static bool Equals(const cel::BoolValue& lhs, const cel::BoolValue& rhs) {
    return lhs.value() == rhs.value();
  }
};

template <>
struct ValueOfTraits<cel::BytesValue, void> {
  static absl::StatusOr<cel::Handle<cel::BytesValue>> Create(
      cel::ValueFactory& value_factory) {
    return value_factory.GetBytesValue();
  }

  static absl::StatusOr<cel::Handle<cel::BytesValue>> Create(
      cel::ValueFactory& value_factory, absl::string_view value) {
    return value_factory.CreateBytesValue(value);
  }

  static absl::StatusOr<cel::Handle<cel::BytesValue>> Create(
      cel::ValueFactory& value_factory, absl::Cord value) {
    return value_factory.CreateBytesValue(std::move(value));
  }

  static bool Equals(const cel::BytesValue& lhs, const cel::BytesValue& rhs) {
    return lhs.Equals(rhs);
  }
};

template <>
struct ValueOfTraits<cel::DoubleValue, void> {
  static absl::StatusOr<cel::Handle<cel::DoubleValue>> Create(
      cel::ValueFactory& value_factory, double value) {
    return value_factory.CreateDoubleValue(value);
  }

  static bool Equals(const cel::DoubleValue& lhs, const cel::DoubleValue& rhs) {
    return lhs.value() == rhs.value();
  }
};

template <>
struct ValueOfTraits<cel::DurationValue, void> {
  static absl::StatusOr<cel::Handle<cel::DurationValue>> Create(
      cel::ValueFactory& value_factory, absl::Duration value) {
    return value_factory.CreateDurationValue(value);
  }

  static bool Equals(const cel::DurationValue& lhs,
                     const cel::DurationValue& rhs) {
    return lhs.value() == rhs.value();
  }
};

template <>
struct ValueOfTraits<cel::IntValue, void> {
  static absl::StatusOr<cel::Handle<cel::IntValue>> Create(
      cel::ValueFactory& value_factory, int64_t value) {
    return value_factory.CreateIntValue(value);
  }

  static bool Equals(const cel::IntValue& lhs, const cel::IntValue& rhs) {
    return lhs.value() == rhs.value();
  }
};

template <>
struct ValueOfTraits<cel::NullValue, void> {
  static absl::StatusOr<cel::Handle<cel::NullValue>> Create(
      cel::ValueFactory& value_factory) {
    return value_factory.GetNullValue();
  }

  static bool Equals(const cel::NullValue& lhs, const cel::NullValue& rhs) {
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;
  }
};

template <>
struct ValueOfTraits<cel::StringValue, void> {
  static absl::StatusOr<cel::Handle<cel::StringValue>> Create(
      cel::ValueFactory& value_factory) {
    return value_factory.GetStringValue();
  }

  static absl::StatusOr<cel::Handle<cel::StringValue>> Create(
      cel::ValueFactory& value_factory, absl::string_view value) {
    return value_factory.CreateStringValue(value);
  }

  static absl::StatusOr<cel::Handle<cel::StringValue>> Create(
      cel::ValueFactory& value_factory, absl::Cord value) {
    return value_factory.CreateStringValue(std::move(value));
  }

  static bool Equals(const cel::StringValue& lhs, const cel::StringValue& rhs) {
    return lhs.Equals(rhs);
  }
};

template <>
struct ValueOfTraits<cel::TimestampValue, void> {
  static absl::StatusOr<cel::Handle<cel::TimestampValue>> Create(
      cel::ValueFactory& value_factory, absl::Time value) {
    return value_factory.CreateTimestampValue(value);
  }

  static bool Equals(const cel::TimestampValue& lhs,
                     const cel::TimestampValue& rhs) {
    return lhs.value() == rhs.value();
  }
};

template <>
struct ValueOfTraits<cel::UintValue, void> {
  static absl::StatusOr<cel::Handle<cel::UintValue>> Create(
      cel::ValueFactory& value_factory, uint64_t value) {
    return value_factory.CreateUintValue(value);
  }

  static bool Equals(const cel::UintValue& lhs, const cel::UintValue& rhs) {
    return lhs.value() == rhs.value();
  }
};

template <>
struct ValueOfTraits<cel::TypeValue, void> {
  static absl::StatusOr<cel::Handle<cel::TypeValue>> Create(
      cel::ValueFactory& value_factory, cel::Handle<cel::Type> type) {
    return value_factory.CreateTypeValue(std::move(type));
  }

  static bool Equals(const cel::TypeValue& lhs, const cel::TypeValue& rhs) {
    return lhs.name() == rhs.name();
  }
};

template <>
struct ValueOfTraits<cel::ErrorValue, void> {
  static absl::StatusOr<cel::Handle<cel::ErrorValue>> Create(
      cel::ValueFactory& value_factory, absl::Status status) {
    return value_factory.CreateErrorValue(std::move(status));
  }

  static bool Equals(const cel::ErrorValue& lhs, const cel::ErrorValue& rhs) {
    return lhs.value() == rhs.value();
  }
};

template <typename T>
class ValueOfImpl {
 public:
  explicit ValueOfImpl(cel::Handle<T> value) : value_(std::move(value)) {}

  template <typename U>
  operator testing::Matcher<U>() const {  // NOLINT(google-explicit-constructor)
    return testing::Matcher<U>(new Impl<const U&>(value_));
  }

 private:
  template <typename U>
  class Impl final : public testing::MatcherInterface<U> {
   public:
    explicit Impl(cel::Handle<T> value) : value_(std::move(value)) {}

    void DescribeTo(std::ostream* os) const override {
      *os << "is an instance of " << value_->type()->DebugString()
          << " equal to " << value_->DebugString();
    }

    void DescribeNegationTo(std::ostream* os) const override {
      *os << "is not an instance of " << value_->type()->DebugString()
          << " equal to " << value_->DebugString();
    }

    bool MatchAndExplain(
        U u, testing::MatchResultListener* listener) const override {
      if (!IndirectImpl(u).template Is<T>()) {
        return false;
      }
      if (!ValueOfTraits<T>::Equals(T::Cast(IndirectImpl(u)), *value_)) {
        return false;
      }
      *listener << "which is an instance of " << value_->type()->DebugString()
                << " and equal to " << value_->DebugString();
      return true;
    }

    const cel::Handle<T> value_;
  };

  const cel::Handle<T> value_;
};

}  // namespace base_internal

// ValueIs<T>() tests that the subject is an instance of T, using
// cel::Value::Is<T>().
//
// Usage:
//
// EXPECT_THAT(foo, ValueIs<IntValue>());
template <typename T>
base_internal::ValueIsImpl<T> ValueIs() {
  static_assert(std::is_base_of_v<cel::Value, T>);
  return base_internal::ValueIsImpl<T>();
}

// ValueOf<T>() tests that the subject is an instance of T and equal to the
// instance T.
//
// Usage:
//
// ValueFactory& value_factory = ...;
// EXPECT_THAT(foo, ValueOf<IntValue>(value_factory, 1));
template <typename T, typename... Args>
base_internal::ValueOfImpl<T> ValueOf(cel::ValueFactory& value_factory,
                                      Args&&... args) {
  static_assert(std::is_base_of_v<cel::Value, T>);
  auto status_or_value = base_internal::ValueOfTraits<T>::Create(
      value_factory, std::forward<Args>(args)...);
  ABSL_CHECK_OK(status_or_value);  // Crask OK
  return base_internal::ValueOfImpl<T>(std::move(status_or_value).value());
}

}  // namespace cel_testing

#endif  // THIRD_PARTY_CEL_CPP_BASE_TESTING_VALUE_MATCHERS_H_
