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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_UINT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_UINT_VALUE_H_

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
class UintValue;
class UintValueView;
class TypeManager;

namespace common_internal {

struct UintValueBase {
  static constexpr ValueKind kKind = ValueKind::kUint;

  constexpr explicit UintValueBase(uint64_t value) noexcept : value(value) {}

  UintValueBase() = default;
  UintValueBase(const UintValueBase&) = default;
  UintValueBase(UintValueBase&&) = default;
  UintValueBase& operator=(const UintValueBase&) = default;
  UintValueBase& operator=(UintValueBase&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  UintType GetType(TypeManager&) const { return UintType(); }

  absl::string_view GetTypeName() const { return UintType::kName; }

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

  bool IsZeroValue() const { return NativeValue() == 0; }

  constexpr uint64_t NativeValue() const { return value; }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator uint64_t() const noexcept { return value; }

  uint64_t value = 0;
};

}  // namespace common_internal

// `UintValue` represents values of the primitive `uint` type.
class UintValue final : private common_internal::UintValueBase {
 private:
  using Base = UintValueBase;

 public:
  using view_alternative_type = UintValueView;

  using Base::kKind;

  UintValue() = default;
  UintValue(const UintValue&) = default;
  UintValue(UintValue&&) = default;
  UintValue& operator=(const UintValue&) = default;
  UintValue& operator=(UintValue&&) = default;

  constexpr explicit UintValue(uint64_t value) noexcept : Base(value) {}

  constexpr explicit UintValue(UintValueView other) noexcept;

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

  using Base::operator uint64_t;

  friend void swap(UintValue& lhs, UintValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class UintValueView;
};

template <typename H>
H AbslHashValue(H state, UintValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(UintValue lhs, UintValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(UintValue lhs, UintValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, UintValue value) {
  return out << value.DebugString();
}

class UintValueView final : private common_internal::UintValueBase {
 private:
  using Base = UintValueBase;

 public:
  using alternative_type = UintValue;

  using Base::kKind;

  UintValueView() = default;
  UintValueView(const UintValueView&) = default;
  UintValueView(UintValueView&&) = default;
  UintValueView& operator=(const UintValueView&) = default;
  UintValueView& operator=(UintValueView&&) = default;

  constexpr explicit UintValueView(uint64_t value) noexcept : Base(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr UintValueView(UintValue other) noexcept
      : UintValueView(static_cast<uint64_t>(other)) {}

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

  using Base::operator uint64_t;

  friend void swap(UintValueView& lhs, UintValueView& rhs) noexcept {
    using std::swap;
    swap(lhs.value, rhs.value);
  }

 private:
  friend class IntValue;
};

template <typename H>
H AbslHashValue(H state, UintValueView value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(UintValueView lhs, UintValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(UintValueView lhs, UintValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, UintValueView value) {
  return out << value.DebugString();
}

inline constexpr UintValue::UintValue(UintValueView other) noexcept
    : UintValue(static_cast<uint64_t>(other)) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_UINT_VALUE_H_
