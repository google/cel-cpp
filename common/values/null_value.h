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

// `NullValue` represents values of the primitive `duration` type.
class NullValue final {
 public:
  using view_alternative_type = NullValueView;

  static constexpr ValueKind kKind = ValueKind::kNull;

  explicit NullValue(NullValueView) noexcept;

  NullValue() = default;
  NullValue(const NullValue&) = default;
  NullValue(NullValue&&) = default;
  NullValue& operator=(const NullValue&) = default;
  NullValue& operator=(NullValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  NullType GetType(TypeManager&) const { return NullType(); }

  absl::string_view GetTypeName() const { return NullType::kName; }

  std::string DebugString() const { return "null"; }

  absl::StatusOr<size_t> GetSerializedSize() const;

  absl::Status SerializeTo(absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize() const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const { return kJsonNull; }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return true; }

  constexpr void swap(NullValue& other) noexcept {}

 private:
  friend class NullValueView;
};

inline constexpr void swap(NullValue& lhs, NullValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, const NullValue& value) {
  return out << value.DebugString();
}

class NullValueView final {
 public:
  using alternative_type = NullValue;

  static constexpr ValueKind kKind = NullValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  NullValueView(const NullValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND
                    ABSL_ATTRIBUTE_UNUSED) noexcept {}

  NullValueView() = default;
  NullValueView(const NullValueView&) = default;
  NullValueView(NullValueView&&) = default;
  NullValueView& operator=(const NullValueView&) = default;
  NullValueView& operator=(NullValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  NullType GetType(TypeManager&) const { return NullType(); }

  absl::string_view GetTypeName() const { return NullType::kName; }

  std::string DebugString() const { return "null"; }

  absl::StatusOr<size_t> GetSerializedSize() const;

  absl::Status SerializeTo(absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize() const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const { return kJsonNull; }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return true; }

  constexpr void swap(NullValueView& other) noexcept {}

 private:
  friend class NullValue;
};

inline constexpr void swap(NullValueView& lhs, NullValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, NullValueView value) {
  return out << value.DebugString();
}

inline NullValue::NullValue(NullValueView) noexcept {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_NULL_VALUE_H_
