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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_

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
class BoolValue;
class BoolValueView;
class TypeManager;

namespace common_internal {

struct BoolValueBase {
  static constexpr ValueKind kKind = ValueKind::kBool;

  BoolValueBase() = default;
  BoolValueBase(const BoolValueBase&) = default;
  BoolValueBase(BoolValueBase&&) = default;
  BoolValueBase& operator=(const BoolValueBase&) = default;
  BoolValueBase& operator=(BoolValueBase&&) = default;

  constexpr explicit BoolValueBase(bool value) noexcept : value(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator bool() const noexcept { return value; }

  constexpr ValueKind kind() const { return kKind; }

  BoolType GetType(TypeManager&) const { return BoolType(); }

  absl::string_view GetTypeName() const { return BoolType::kName; }

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

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return NativeValue() == false; }

  constexpr bool NativeValue() const { return value; }

  bool value = false;
};

}  // namespace common_internal

// `BoolValue` represents values of the primitive `bool` type.
class BoolValue final : private common_internal::BoolValueBase {
 private:
  using Base = BoolValueBase;

 public:
  using view_alternative_type = BoolValueView;

  using Base::kKind;

  BoolValue() = default;
  BoolValue(const BoolValue&) = default;
  BoolValue(BoolValue&&) = default;
  BoolValue& operator=(const BoolValue&) = default;
  BoolValue& operator=(BoolValue&&) = default;

  constexpr explicit BoolValue(bool value) noexcept : Base(value) {}

  constexpr explicit BoolValue(BoolValueView other) noexcept;

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

  using Base::operator bool;

  friend void swap(BoolValue& lhs, BoolValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class BoolValueView;
};

template <typename H>
H AbslHashValue(H state, BoolValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

inline std::ostream& operator<<(std::ostream& out, BoolValue value) {
  return out << value.DebugString();
}

class BoolValueView final : private common_internal::BoolValueBase {
 private:
  using Base = BoolValueBase;

 public:
  using alternative_type = BoolValue;

  using Base::kKind;

  BoolValueView() = default;
  BoolValueView(const BoolValueView&) = default;
  BoolValueView(BoolValueView&&) = default;
  BoolValueView& operator=(const BoolValueView&) = default;
  BoolValueView& operator=(BoolValueView&&) = default;

  constexpr explicit BoolValueView(bool value) noexcept : Base(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr BoolValueView(BoolValue other) noexcept
      : BoolValueView(static_cast<bool>(other)) {}

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

  using Base::operator bool;

  friend void swap(BoolValueView& lhs, BoolValueView& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class BoolValue;
};

template <typename H>
H AbslHashValue(H state, BoolValueView value) {
  return H::combine(std::move(state), value.NativeValue());
}

inline std::ostream& operator<<(std::ostream& out, BoolValueView value) {
  return out << value.DebugString();
}

inline constexpr BoolValue::BoolValue(BoolValueView other) noexcept
    : BoolValue(static_cast<bool>(other)) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
