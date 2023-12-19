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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class DoubleValue;
class DoubleValueView;

// `DoubleValue` represents values of the primitive `double` type.
class DoubleValue final {
 public:
  using view_alternative_type = DoubleValueView;

  static constexpr ValueKind kKind = ValueKind::kDouble;

  constexpr explicit DoubleValue(double value) noexcept : value_(value) {}

  constexpr explicit DoubleValue(DoubleValueView value) noexcept;

  DoubleValue() = default;
  DoubleValue(const DoubleValue&) = default;
  DoubleValue(DoubleValue&&) = default;
  DoubleValue& operator=(const DoubleValue&) = default;
  DoubleValue& operator=(DoubleValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DoubleTypeView type() const { return DoubleTypeView(); }

  std::string DebugString() const;

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. This always
  // succeeds and only returns `absl::StatusOr` to meet concept requirements.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` serializes this value and returns it as `absl::Cord`.
  absl::StatusOr<absl::Cord> Serialize() const;

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // 'ConvertToAny' converts this value to `Any`.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  constexpr double NativeValue() const { return value_; }

  void swap(DoubleValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DoubleValueView;

  double value_ = 0.0;
};

inline void swap(DoubleValue& lhs, DoubleValue& rhs) noexcept { lhs.swap(rhs); }

constexpr bool operator==(DoubleValue lhs, DoubleValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(DoubleValue lhs, double rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(double lhs, DoubleValue rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator!=(DoubleValue lhs, DoubleValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DoubleValue lhs, double rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(double lhs, DoubleValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DoubleValue value) {
  return out << value.DebugString();
}

class DoubleValueView final {
 public:
  using alternative_type = DoubleValue;

  static constexpr ValueKind kKind = DoubleValue::kKind;

  constexpr explicit DoubleValueView(double value) noexcept : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DoubleValueView(DoubleValue value) noexcept
      : DoubleValueView(value.value_) {}

  DoubleValueView() = default;
  DoubleValueView(const DoubleValueView&) = default;
  DoubleValueView(DoubleValueView&&) = default;
  DoubleValueView& operator=(const DoubleValueView&) = default;
  DoubleValueView& operator=(DoubleValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DoubleTypeView type() const { return DoubleTypeView(); }

  std::string DebugString() const;

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. This always
  // succeeds and only returns `absl::StatusOr` to meet concept requirements.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` serializes this value and returns it as `absl::Cord`.
  absl::StatusOr<absl::Cord> Serialize() const;

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // 'ConvertToAny' converts this value to `Any`.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  constexpr double NativeValue() const { return value_; }

  void swap(DoubleValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DoubleValue;

  // We pass around by value, as its cheaper than passing around a pointer due
  // to the performance degradation from pointer chasing.
  double value_ = 0.0;
};

inline void swap(DoubleValueView& lhs, DoubleValueView& rhs) noexcept {
  lhs.swap(rhs);
}

constexpr bool operator==(DoubleValueView lhs, DoubleValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(DoubleValueView lhs, double rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(double lhs, DoubleValueView rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator==(DoubleValueView lhs, DoubleValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(DoubleValue lhs, DoubleValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(DoubleValueView lhs, DoubleValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DoubleValueView lhs, double rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(double lhs, DoubleValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DoubleValueView lhs, DoubleValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DoubleValue lhs, DoubleValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DoubleValueView value) {
  return out << value.DebugString();
}

inline constexpr DoubleValue::DoubleValue(DoubleValueView value) noexcept
    : DoubleValue(value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_
