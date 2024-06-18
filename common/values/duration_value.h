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
class DurationValue;
class DurationValueView;
class TypeManager;

namespace common_internal {

struct DurationValueBase {
  static constexpr ValueKind kKind = ValueKind::kDuration;

  constexpr explicit DurationValueBase(absl::Duration value) noexcept
      : value(value) {}

  DurationValueBase() = default;
  DurationValueBase(const DurationValueBase&) = default;
  DurationValueBase(DurationValueBase&&) = default;
  DurationValueBase& operator=(const DurationValueBase&) = default;
  DurationValueBase& operator=(DurationValueBase&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DurationType GetType(TypeManager&) const { return DurationType(); }

  absl::string_view GetTypeName() const { return DurationType::kName; }

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

  bool IsZeroValue() const { return NativeValue() == absl::ZeroDuration(); }

  constexpr absl::Duration NativeValue() const { return value; }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator absl::Duration() const noexcept { return value; }

  absl::Duration value = absl::ZeroDuration();
};

}  // namespace common_internal

// `DurationValue` represents values of the primitive `duration` type.
class DurationValue final : private common_internal::DurationValueBase {
 private:
  using Base = DurationValueBase;

 public:
  using view_alternative_type = DurationValueView;

  using Base::kKind;

  DurationValue() = default;
  DurationValue(const DurationValue&) = default;
  DurationValue(DurationValue&&) = default;
  DurationValue& operator=(const DurationValue&) = default;
  DurationValue& operator=(DurationValue&&) = default;

  constexpr explicit DurationValue(absl::Duration value) noexcept
      : Base(value) {}

  constexpr explicit DurationValue(DurationValueView other) noexcept;

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

  using Base::operator absl::Duration;

  friend void swap(DurationValue& lhs, DurationValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class DurationValueView;
};

inline bool operator==(DurationValue lhs, DurationValue rhs) {
  return static_cast<absl::Duration>(lhs) == static_cast<absl::Duration>(rhs);
}

inline bool operator!=(DurationValue lhs, DurationValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValue value) {
  return out << value.DebugString();
}

class DurationValueView final : private common_internal::DurationValueBase {
 private:
  using Base = DurationValueBase;

 public:
  using alternative_type = DurationValue;

  using Base::kKind;

  DurationValueView() = default;
  DurationValueView(const DurationValueView&) = default;
  DurationValueView(DurationValueView&&) = default;
  DurationValueView& operator=(const DurationValueView&) = default;
  DurationValueView& operator=(DurationValueView&&) = default;

  constexpr explicit DurationValueView(absl::Duration value) noexcept
      : Base(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DurationValueView(DurationValue other) noexcept
      : DurationValueView(static_cast<absl::Duration>(other)) {}

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

  using Base::operator absl::Duration;

  friend void swap(DurationValueView& lhs, DurationValueView& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class DurationValue;
};

inline bool operator==(DurationValueView lhs, DurationValueView rhs) {
  return static_cast<absl::Duration>(lhs) == static_cast<absl::Duration>(rhs);
}

inline bool operator!=(DurationValueView lhs, DurationValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValueView value) {
  return out << value.DebugString();
}

inline constexpr DurationValue::DurationValue(DurationValueView other) noexcept
    : DurationValue(static_cast<absl::Duration>(other)) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
