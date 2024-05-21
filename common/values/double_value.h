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

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ValueView;
class ValueManager;
class DoubleValue;
class DoubleValueView;
class TypeManager;

namespace common_internal {

struct DoubleValueBase {
  static constexpr ValueKind kKind = ValueKind::kDouble;

  constexpr explicit DoubleValueBase(double value) noexcept : value(value) {}

  DoubleValueBase() = default;
  DoubleValueBase(const DoubleValueBase&) = default;
  DoubleValueBase(DoubleValueBase&&) = default;
  DoubleValueBase& operator=(const DoubleValueBase&) = default;
  DoubleValueBase& operator=(DoubleValueBase&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DoubleType GetType(TypeManager&) const { return DoubleType(); }

  absl::string_view GetTypeName() const { return DoubleType::kName; }

  std::string DebugString() const;

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. This always
  // succeeds and only returns `absl::StatusOr` to meet concept requirements.
  absl::StatusOr<size_t> GetSerializedSize(AnyToJsonConverter&) const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  // `Serialize` serializes this value and returns it as `absl::Cord`.
  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter& value_manager) const;

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // 'ConvertToAny' converts this value to `Any`.
  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter&,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              ValueView other) const;

  bool IsZeroValue() const { return NativeValue() == 0.0; }

  constexpr double NativeValue() const { return value; }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator double() const noexcept { return value; }

  double value = 0.0;
};

}  // namespace common_internal

// `DoubleValue` represents values of the primitive `double` type.
class DoubleValue final : private common_internal::DoubleValueBase {
 private:
  using Base = DoubleValueBase;

 public:
  using view_alternative_type = DoubleValueView;

  using Base::kKind;

  DoubleValue() = default;
  DoubleValue(const DoubleValue&) = default;
  DoubleValue(DoubleValue&&) = default;
  DoubleValue& operator=(const DoubleValue&) = default;
  DoubleValue& operator=(DoubleValue&&) = default;

  constexpr explicit DoubleValue(double value) noexcept : Base(value) {}

  constexpr explicit DoubleValue(DoubleValueView other) noexcept;

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

  using Base::operator double;

  friend void swap(DoubleValue& lhs, DoubleValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class DoubleValueView;
};

inline std::ostream& operator<<(std::ostream& out, DoubleValue value) {
  return out << value.DebugString();
}

class DoubleValueView final : private common_internal::DoubleValueBase {
 private:
  using Base = DoubleValueBase;

 public:
  using alternative_type = DoubleValue;

  using Base::kKind;

  DoubleValueView() = default;
  DoubleValueView(const DoubleValueView&) = default;
  DoubleValueView(DoubleValueView&&) = default;
  DoubleValueView& operator=(const DoubleValueView&) = default;
  DoubleValueView& operator=(DoubleValueView&&) = default;

  constexpr explicit DoubleValueView(double value) noexcept : Base(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DoubleValueView(DoubleValue other) noexcept
      : DoubleValueView(static_cast<double>(other)) {}

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

  using Base::operator double;

  friend void swap(DoubleValueView& lhs, DoubleValueView& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class BoolValue;
};

inline std::ostream& operator<<(std::ostream& out, DoubleValueView value) {
  return out << value.DebugString();
}

inline constexpr DoubleValue::DoubleValue(DoubleValueView other) noexcept
    : DoubleValue(static_cast<double>(other)) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_
