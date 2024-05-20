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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_NULL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_NULL_VALUE_H_

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
class NullValue;
class NullValueView;
class TypeManager;

namespace common_internal {

struct NullValueBase {
  static constexpr ValueKind kKind = ValueKind::kNull;

  NullValueBase() = default;
  NullValueBase(const NullValueBase&) = default;
  NullValueBase(NullValueBase&&) = default;
  NullValueBase& operator=(const NullValueBase&) = default;
  NullValueBase& operator=(NullValueBase&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  NullType GetType(TypeManager&) const { return NullType(); }

  absl::string_view GetTypeName() const { return NullType::kName; }

  std::string DebugString() const { return "null"; }

  absl::StatusOr<size_t> GetSerializedSize(AnyToJsonConverter&) const;

  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter&) const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter&,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const {
    return kJsonNull;
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              ValueView other) const;

  bool IsZeroValue() const { return true; }
};

}  // namespace common_internal

// `NullValue` represents values of the primitive `duration` type.
class NullValue final : private common_internal::NullValueBase {
 private:
  using Base = NullValueBase;

 public:
  using view_alternative_type = NullValueView;

  using Base::kKind;

  NullValue() = default;
  NullValue(const NullValue&) = default;
  NullValue(NullValue&&) = default;
  NullValue& operator=(const NullValue&) = default;
  NullValue& operator=(NullValue&&) = default;

  constexpr explicit NullValue(NullValueView other) noexcept;

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

  friend void swap(NullValue&, NullValue&) noexcept {}

 private:
  friend class NullValueView;
};

inline bool operator==(NullValue, NullValue) { return true; }

inline bool operator!=(NullValue lhs, NullValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, const NullValue& value) {
  return out << value.DebugString();
}

class NullValueView final : private common_internal::NullValueBase {
 private:
  using Base = NullValueBase;

 public:
  using alternative_type = NullValue;

  using Base::kKind;

  NullValueView() = default;
  NullValueView(const NullValueView&) = default;
  NullValueView(NullValueView&&) = default;
  NullValueView& operator=(const NullValueView&) = default;
  NullValueView& operator=(NullValueView&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr NullValueView(NullValue) noexcept {}

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

  friend void swap(NullValueView&, NullValueView&) noexcept {}

 private:
  friend class NullValue;
};

inline bool operator==(NullValueView, NullValueView) { return true; }

inline bool operator!=(NullValueView lhs, NullValueView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, NullValueView value) {
  return out << value.DebugString();
}

inline constexpr NullValue::NullValue(NullValueView) noexcept {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_NULL_VALUE_H_
