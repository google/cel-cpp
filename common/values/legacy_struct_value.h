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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/struct_value_interface.h"

namespace cel {

class Value;
class ValueView;
class ValueManager;
class TypeManager;

namespace common_internal {

class LegacyStructValue;
class LegacyStructValueView;

// `LegacyStructValue` is a wrapper around the old representation of protocol
// buffer messages in `google::api::expr::runtime::CelValue`. It only supports
// arena allocation.
class LegacyStructValue final {
 public:
  using view_alternative_type = LegacyStructValueView;

  static constexpr ValueKind kKind = ValueKind::kStruct;

  explicit LegacyStructValue(LegacyStructValueView value);

  LegacyStructValue(uintptr_t message_ptr, uintptr_t type_info)
      : message_ptr_(message_ptr), type_info_(type_info) {}

  LegacyStructValue(const LegacyStructValue&) = default;
  LegacyStructValue& operator=(const LegacyStructValue&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StructType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::StatusOr<size_t> GetSerializedSize() const;

  absl::Status SerializeTo(absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize() const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  absl::StatusOr<JsonObject> ConvertToJsonObject() const;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const;

  void swap(LegacyStructValue& other) noexcept {
    using std::swap;
    swap(message_ptr_, other.message_ptr_);
    swap(type_info_, other.type_info_);
  }

  absl::StatusOr<ValueView> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<std::pair<ValueView, int>> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  friend class LegacyStructValueView;

  uintptr_t message_ptr_;
  uintptr_t type_info_;
};

inline void swap(LegacyStructValue& lhs, LegacyStructValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const LegacyStructValue& value) {
  return out << value.DebugString();
}

// `LegacyStructValue` is a wrapper around the old representation of protocol
// buffer messages in `google::api::expr::runtime::CelValue`. It only supports
// arena allocation.
class LegacyStructValueView final {
 public:
  using alternative_type = LegacyStructValue;

  static constexpr ValueKind kKind = LegacyStructValue::kKind;

  LegacyStructValueView(uintptr_t message_ptr, uintptr_t type_info)
      : message_ptr_(message_ptr), type_info_(type_info) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  LegacyStructValueView(
      const LegacyStructValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : LegacyStructValueView(value.message_ptr_, value.type_info_) {}

  LegacyStructValueView(LegacyStructValue&&) = delete;

  LegacyStructValueView(const LegacyStructValueView&) = default;
  LegacyStructValueView& operator=(const LegacyStructValueView&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  LegacyStructValueView& operator=(
      const LegacyStructValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    message_ptr_ = value.message_ptr_;
    type_info_ = value.type_info_;
    return *this;
  }

  LegacyStructValueView& operator=(LegacyStructValue&&) = delete;

  constexpr ValueKind kind() const { return kKind; }

  StructType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::StatusOr<size_t> GetSerializedSize() const;

  absl::Status SerializeTo(absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize() const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  absl::StatusOr<JsonObject> ConvertToJsonObject() const;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const;

  void swap(LegacyStructValueView& other) noexcept {
    using std::swap;
    swap(message_ptr_, other.message_ptr_);
    swap(type_info_, other.type_info_);
  }

  absl::StatusOr<ValueView> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<std::pair<ValueView, int>> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  friend class LegacyStructValue;

  uintptr_t message_ptr_;
  uintptr_t type_info_;
};

inline void swap(LegacyStructValueView& lhs,
                 LegacyStructValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                LegacyStructValueView value) {
  return out << value.DebugString();
}

inline LegacyStructValue::LegacyStructValue(LegacyStructValueView value)
    : message_ptr_(value.message_ptr_), type_info_(value.type_info_) {}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_
