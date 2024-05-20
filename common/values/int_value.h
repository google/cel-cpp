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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_

#include <cstddef>
#include <cstdint>
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
class IntValue;
class IntValueView;
class TypeManager;

namespace common_internal {

struct IntValueBase {
  static constexpr ValueKind kKind = ValueKind::kInt;

  constexpr explicit IntValueBase(int64_t value) noexcept : value(value) {}

  IntValueBase() = default;
  IntValueBase(const IntValueBase&) = default;
  IntValueBase(IntValueBase&&) = default;
  IntValueBase& operator=(const IntValueBase&) = default;
  IntValueBase& operator=(IntValueBase&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  IntType GetType(TypeManager&) const { return IntType(); }

  absl::string_view GetTypeName() const { return IntType::kName; }

  std::string DebugString() const;

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. This always
  // succeeds and only returns `absl::StatusOr` to meet concept requirements.
  absl::StatusOr<size_t> GetSerializedSize(AnyToJsonConverter&) const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  // `Serialize` serializes this value and returns it as `absl::Cord`.
  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter&) const;

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

  bool IsZeroValue() const { return NativeValue() == 0; }

  constexpr int64_t NativeValue() const { return value; }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator int64_t() const noexcept { return value; }

  int64_t value = 0;
};

}  // namespace common_internal

// `IntValue` represents values of the primitive `int` type.
class IntValue final : private common_internal::IntValueBase {
 private:
  using Base = IntValueBase;

 public:
  using view_alternative_type = IntValueView;

  using Base::kKind;

  IntValue() = default;
  IntValue(const IntValue&) = default;
  IntValue(IntValue&&) = default;
  IntValue& operator=(const IntValue&) = default;
  IntValue& operator=(IntValue&&) = default;

  constexpr explicit IntValue(int64_t value) noexcept : Base(value) {}

  constexpr explicit IntValue(IntValueView other) noexcept;

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

  using Base::operator int64_t;

  friend void swap(IntValue& lhs, IntValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class IntValueView;
};

template <typename H>
H AbslHashValue(H state, IntValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(IntValue lhs, IntValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(IntValue lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, IntValue value) {
  return out << value.DebugString();
}

class IntValueView final : private common_internal::IntValueBase {
 private:
  using Base = IntValueBase;

 public:
  using alternative_type = IntValue;

  using Base::kKind;

  IntValueView() = default;
  IntValueView(const IntValueView&) = default;
  IntValueView(IntValueView&&) = default;
  IntValueView& operator=(const IntValueView&) = default;
  IntValueView& operator=(IntValueView&&) = default;

  constexpr explicit IntValueView(int64_t value) noexcept : Base(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr IntValueView(IntValue other) noexcept
      : IntValueView(static_cast<int64_t>(other)) {}

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

  using Base::operator int64_t;

  friend void swap(IntValueView& lhs, IntValueView& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class IntValue;
};

template <typename H>
H AbslHashValue(H state, IntValueView value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(IntValueView lhs, IntValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(IntValueView lhs, IntValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, IntValueView value) {
  return out << value.DebugString();
}

inline constexpr IntValue::IntValue(IntValueView other) noexcept
    : IntValue(static_cast<int64_t>(other)) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
