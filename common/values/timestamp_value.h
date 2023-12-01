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

#include <ostream>
#include <string>

#include "absl/time/time.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class TimestampValue;
class TimestampValueView;

// `TimestampValue` represents values of the primitive `timestamp` type.
class TimestampValue final {
 public:
  using view_alternative_type = TimestampValueView;

  static constexpr ValueKind kKind = ValueKind::kTimestamp;

  constexpr explicit TimestampValue(absl::Time value) noexcept
      : value_(value) {}

  constexpr explicit TimestampValue(TimestampValueView value) noexcept;

  TimestampValue() = default;
  TimestampValue(const TimestampValue&) = default;
  TimestampValue(TimestampValue&&) = default;
  TimestampValue& operator=(const TimestampValue&) = default;
  TimestampValue& operator=(TimestampValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  TimestampTypeView type() const { return TimestampTypeView(); }

  std::string DebugString() const;

  constexpr absl::Time NativeValue() const { return value_; }

  void swap(TimestampValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class TimestampValueView;

  absl::Time value_ = absl::UnixEpoch();
};

inline void swap(TimestampValue& lhs, TimestampValue& rhs) noexcept {
  lhs.swap(rhs);
}

constexpr bool operator==(TimestampValue lhs, TimestampValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(TimestampValue lhs, absl::Time rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(absl::Time lhs, TimestampValue rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator!=(TimestampValue lhs, TimestampValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TimestampValue lhs, absl::Time rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(absl::Time lhs, TimestampValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, TimestampValue value) {
  return out << value.DebugString();
}

class TimestampValueView final {
 public:
  using alternative_type = TimestampValue;

  static constexpr ValueKind kKind = TimestampValue::kKind;

  constexpr explicit TimestampValueView(absl::Time value) noexcept
      : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr TimestampValueView(TimestampValue value) noexcept
      : TimestampValueView(value.value_) {}

  TimestampValueView() = default;
  TimestampValueView(const TimestampValueView&) = default;
  TimestampValueView(TimestampValueView&&) = default;
  TimestampValueView& operator=(const TimestampValueView&) = default;
  TimestampValueView& operator=(TimestampValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  TimestampTypeView type() const { return TimestampTypeView(); }

  std::string DebugString() const;

  constexpr absl::Time NativeValue() const { return value_; }

  void swap(TimestampValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class TimestampValue;

  // We pass around by value, as its cheaper than passing around a pointer due
  // to the performance degradation from pointer chasing.
  absl::Time value_ = absl::UnixEpoch();
};

inline void swap(TimestampValueView& lhs, TimestampValueView& rhs) noexcept {
  lhs.swap(rhs);
}

constexpr bool operator==(TimestampValueView lhs, TimestampValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(TimestampValueView lhs, absl::Time rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(absl::Time lhs, TimestampValueView rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator==(TimestampValueView lhs, TimestampValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(TimestampValue lhs, TimestampValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(TimestampValueView lhs, TimestampValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TimestampValueView lhs, absl::Time rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(absl::Time lhs, TimestampValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TimestampValueView lhs, TimestampValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TimestampValue lhs, TimestampValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, TimestampValueView value) {
  return out << value.DebugString();
}

inline constexpr TimestampValue::TimestampValue(
    TimestampValueView value) noexcept
    : TimestampValue(value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_TIMESTAMP_VALUE_H_
