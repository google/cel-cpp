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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_ERROR_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_ERROR_VALUE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class ErrorValue;
class ErrorValueView;
class TypeManager;

// `ErrorValue` represents values of the `ErrorType`.
class ABSL_ATTRIBUTE_TRIVIAL_ABI ErrorValue final {
 public:
  using view_alternative_type = ErrorValueView;

  static constexpr ValueKind kKind = ValueKind::kError;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ErrorValue(absl::Status value) noexcept : value_(std::move(value)) {
    ABSL_DCHECK(!value_.ok()) << "ErrorValue requires a non-OK absl::Status";
  }

  explicit ErrorValue(ErrorValueView value) noexcept;

  // By default, this creates an UNKNOWN error. You should always create a more
  // specific error value.
  ErrorValue();
  ErrorValue(const ErrorValue&) = default;

  ErrorValue& operator=(const ErrorValue&) = default;

  ErrorValue(ErrorValue&& other) noexcept : value_(std::move(other.value_)) {}

  ErrorValue& operator=(ErrorValue&& other) noexcept {
    value_ = std::move(other.value_);
    return *this;
  }

  constexpr ValueKind kind() const { return kKind; }

  ErrorType GetType(TypeManager&) const { return ErrorType(); }

  absl::string_view GetTypeName() const { return ErrorType::kName; }

  std::string DebugString() const;

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `ErrorValue` is
  // not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  absl::Status NativeValue() const {
    ABSL_DCHECK(!value_.ok()) << "use of moved-from ErrorValue";
    return value_;
  }

  void swap(ErrorValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class ErrorValueView;

  absl::Status value_;
};

inline void swap(ErrorValue& lhs, ErrorValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const ErrorValue& value) {
  return out << value.DebugString();
}

class ErrorValueView final {
 public:
  using alternative_type = ErrorValue;

  static constexpr ValueKind kKind = ErrorValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ErrorValueView(
      const absl::Status& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : value_(std::addressof(value)) {
    ABSL_DCHECK(!value.ok()) << "ErrorValueView requires a non-OK absl::Status";
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  ErrorValueView(const ErrorValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : value_(std::addressof(value.value_)) {
    ABSL_DCHECK(!value_->ok()) << "use of moved-from ErrorValue";
  }

  // By default, this creates an UNKNOWN error. You should always create a more
  // specific error value.
  ErrorValueView();
  ErrorValueView(const ErrorValueView&) = default;
  ErrorValueView(ErrorValueView&&) = default;
  ErrorValueView& operator=(const ErrorValueView&) = default;
  ErrorValueView& operator=(ErrorValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  ErrorType GetType(TypeManager&) const { return ErrorType(); }

  absl::string_view GetTypeName() const { return ErrorType::kName; }

  std::string DebugString() const;

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `ErrorValue` is
  // not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  absl::Status NativeValue() const {
    ABSL_DCHECK(!value_->ok()) << "use of moved-from ErrorValue";
    return *value_;
  }

  void swap(ErrorValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class ErrorValue;

  absl::Nonnull<const absl::Status*> value_;
};

inline void swap(ErrorValueView& lhs, ErrorValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, ErrorValueView value) {
  return out << value.DebugString();
}

inline ErrorValue::ErrorValue(ErrorValueView value) noexcept
    : value_(value.NativeValue()) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_ERROR_VALUE_H_
