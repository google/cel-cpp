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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class DurationValue;
class DurationValueView;
class TypeManager;

// `DurationValue` represents values of the primitive `duration` type.
class DurationValue final {
 public:
  using view_alternative_type = DurationValueView;

  static constexpr ValueKind kKind = ValueKind::kDuration;

  constexpr explicit DurationValue(absl::Duration value) noexcept
      : value_(value) {}

  constexpr explicit DurationValue(DurationValueView value) noexcept;

  DurationValue() = default;
  DurationValue(const DurationValue&) = default;
  DurationValue(DurationValue&&) = default;
  DurationValue& operator=(const DurationValue&) = default;
  DurationValue& operator=(DurationValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DurationType GetType(TypeManager&) const { return DurationType(); }

  absl::string_view GetTypeName() const { return DurationType::kName; }

  std::string DebugString() const;

  absl::StatusOr<size_t> GetSerializedSize() const;

  absl::Status SerializeTo(absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize() const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  constexpr absl::Duration NativeValue() const { return value_; }

  void swap(DurationValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DurationValueView;

  absl::Duration value_ = absl::ZeroDuration();
};

inline void swap(DurationValue& lhs, DurationValue& rhs) noexcept {
  lhs.swap(rhs);
}

constexpr bool operator==(DurationValue lhs, DurationValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(DurationValue lhs, absl::Duration rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(absl::Duration lhs, DurationValue rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator!=(DurationValue lhs, DurationValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DurationValue lhs, absl::Duration rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(absl::Duration lhs, DurationValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValue value) {
  return out << value.DebugString();
}

class DurationValueView final {
 public:
  using alternative_type = DurationValue;

  static constexpr ValueKind kKind = DurationValue::kKind;

  constexpr explicit DurationValueView(absl::Duration value) noexcept
      : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DurationValueView(DurationValue value) noexcept
      : DurationValueView(value.value_) {}

  DurationValueView() = default;
  DurationValueView(const DurationValueView&) = default;
  DurationValueView(DurationValueView&&) = default;
  DurationValueView& operator=(const DurationValueView&) = default;
  DurationValueView& operator=(DurationValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DurationType GetType(TypeManager&) const { return DurationType(); }

  absl::string_view GetTypeName() const { return DurationType::kName; }

  std::string DebugString() const;

  absl::StatusOr<size_t> GetSerializedSize() const;

  absl::Status SerializeTo(absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize() const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  constexpr absl::Duration NativeValue() const { return value_; }

  void swap(DurationValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DurationValue;

  // We pass around by value, as its cheaper than passing around a pointer due
  // to the performance degradation from pointer chasing.
  absl::Duration value_ = absl::ZeroDuration();
};

inline void swap(DurationValueView& lhs, DurationValueView& rhs) noexcept {
  lhs.swap(rhs);
}

constexpr bool operator==(DurationValueView lhs, DurationValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(DurationValueView lhs, absl::Duration rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(absl::Duration lhs, DurationValueView rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator==(DurationValueView lhs, DurationValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(DurationValue lhs, DurationValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(DurationValueView lhs, DurationValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DurationValueView lhs, absl::Duration rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(absl::Duration lhs, DurationValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DurationValueView lhs, DurationValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(DurationValue lhs, DurationValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValueView value) {
  return out << value.DebugString();
}

inline constexpr DurationValue::DurationValue(DurationValueView value) noexcept
    : DurationValue(value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
