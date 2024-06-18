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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_TIMESTAMP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_TIMESTAMP_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
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

class Value;
class ValueView;
class ValueManager;
class TimestampValue;
class TimestampValueView;
class TypeManager;

namespace common_internal {

struct TimestampValueBase {
  static constexpr ValueKind kKind = ValueKind::kTimestamp;

  constexpr explicit TimestampValueBase(absl::Time value) noexcept
      : value(value) {}

  TimestampValueBase() = default;
  TimestampValueBase(const TimestampValueBase&) = default;
  TimestampValueBase(TimestampValueBase&&) = default;
  TimestampValueBase& operator=(const TimestampValueBase&) = default;
  TimestampValueBase& operator=(TimestampValueBase&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  TimestampType GetType(TypeManager&) const { return TimestampType(); }

  absl::string_view GetTypeName() const { return TimestampType::kName; }

  std::string DebugString() const;

  absl::StatusOr<size_t> GetSerializedSize(AnyToJsonConverter&) const;

  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter&) const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter&,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::Status Equal(ValueManager& value_manager, ValueView other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              ValueView other) const;

  bool IsZeroValue() const { return NativeValue() == absl::UnixEpoch(); }

  constexpr absl::Time NativeValue() const { return value; }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator absl::Time() const noexcept { return value; }

  absl::Time value = absl::UnixEpoch();
};

}  // namespace common_internal

// `TimestampValue` represents values of the primitive `timestamp` type.
class TimestampValue final : private common_internal::TimestampValueBase {
 private:
  using Base = TimestampValueBase;

 public:
  using view_alternative_type = TimestampValueView;

  using Base::kKind;

  TimestampValue() = default;
  TimestampValue(const TimestampValue&) = default;
  TimestampValue(TimestampValue&&) = default;
  TimestampValue& operator=(const TimestampValue&) = default;
  TimestampValue& operator=(TimestampValue&&) = default;

  constexpr explicit TimestampValue(absl::Time value) noexcept : Base(value) {}

  constexpr explicit TimestampValue(TimestampValueView other) noexcept;

  using Base::kind;

  using Base::GetType;

  using Base::GetTypeName;

  using Base::DebugString;

  using Base::GetSerializedSize;

  using Base::SerializeTo;

  using Base::Serialize;

  using Base::GetTypeUrl;

  using Base::ConvertToAny;

  using Base::ConvertToJson;

  using Base::Equal;

  using Base::IsZeroValue;

  using Base::NativeValue;

  using Base::operator absl::Time;

  friend void swap(TimestampValue& lhs, TimestampValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class DurationValueView;
};

constexpr bool operator==(TimestampValue lhs, TimestampValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(TimestampValue lhs, TimestampValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, TimestampValue value) {
  return out << value.DebugString();
}

class TimestampValueView final : private common_internal::TimestampValueBase {
 private:
  using Base = TimestampValueBase;

 public:
  using alternative_type = TimestampValue;

  using Base::kKind;

  TimestampValueView() = default;
  TimestampValueView(const TimestampValueView&) = default;
  TimestampValueView(TimestampValueView&&) = default;
  TimestampValueView& operator=(const TimestampValueView&) = default;
  TimestampValueView& operator=(TimestampValueView&&) = default;

  constexpr explicit TimestampValueView(absl::Time value) noexcept
      : Base(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr TimestampValueView(TimestampValue other) noexcept
      : TimestampValueView(static_cast<absl::Time>(other)) {}

  using Base::kind;

  using Base::GetType;

  using Base::GetTypeName;

  using Base::DebugString;

  using Base::GetSerializedSize;

  using Base::SerializeTo;

  using Base::Serialize;

  using Base::GetTypeUrl;

  using Base::ConvertToAny;

  using Base::ConvertToJson;

  using Base::Equal;

  using Base::IsZeroValue;

  using Base::NativeValue;

  using Base::operator absl::Time;

  friend void swap(TimestampValueView& lhs, TimestampValueView& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class DurationValue;
};

constexpr bool operator==(TimestampValueView lhs, TimestampValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(TimestampValueView lhs, TimestampValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, TimestampValueView value) {
  return out << value.DebugString();
}

inline constexpr TimestampValue::TimestampValue(
    TimestampValueView other) noexcept
    : TimestampValue(static_cast<absl::Time>(other)) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_TIMESTAMP_VALUE_H_
