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

// IWYU pragma: private, include "common/values/list_value.h"
// IWYU pragma: friend "common/values/list_value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_LIST_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/list_value_interface.h"
#include "common/values/values.h"

namespace cel {

class TypeManager;
class ValueManager;
class Value;

namespace common_internal {

class LegacyListValue;

bool Is(const LegacyListValue& lhs, const LegacyListValue& rhs);

class LegacyListValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;

  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit LegacyListValue(uintptr_t impl) : impl_(impl) {}

  // By default, this creates an empty list whose type is `list(dyn)`. Unless
  // you can help it, you should use a more specific typed list value.
  LegacyListValue();
  LegacyListValue(const LegacyListValue&) = default;
  LegacyListValue(LegacyListValue&&) = default;
  LegacyListValue& operator=(const LegacyListValue&) = default;
  LegacyListValue& operator=(LegacyListValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  ListType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const { return "list"; }

  std::string DebugString() const;

  // See `ValueInterface::GetSerializedSize`.
  absl::StatusOr<size_t> GetSerializedSize(
      AnyToJsonConverter& value_manager) const;

  // See `ValueInterface::SerializeTo`.
  absl::Status SerializeTo(AnyToJsonConverter& value_manager,
                           absl::Cord& value) const;

  // See `ValueInterface::Serialize`.
  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter& value_manager) const;

  // See `ValueInterface::GetTypeUrl`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // See `ValueInterface::ConvertToAny`.
  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter& value_manager,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& value_manager) const {
    return ConvertToJsonArray(value_manager);
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& value_manager) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const;

  bool IsZeroValue() const { return IsEmpty(); }

  bool IsEmpty() const;

  size_t Size() const;

  // See LegacyListValueInterface::Get for documentation.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename ListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  void swap(LegacyListValue& other) noexcept {
    using std::swap;
    swap(impl_, other.impl_);
  }

  uintptr_t NativeValue() const { return impl_; }

 private:
  friend bool Is(const LegacyListValue& lhs, const LegacyListValue& rhs);

  uintptr_t impl_;
};

inline void swap(LegacyListValue& lhs, LegacyListValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const LegacyListValue& type) {
  return out << type.DebugString();
}

inline bool Is(const LegacyListValue& lhs, const LegacyListValue& rhs) {
  return lhs.impl_ == rhs.impl_;
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_LIST_VALUE_H_
