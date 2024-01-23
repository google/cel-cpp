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

// IWYU pragma: private, include "common/values/map_value.h"
// IWYU pragma: friend "common/values/map_value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_MAP_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/map_value_interface.h"
#include "common/values/values.h"

namespace cel {

class TypeManager;
class ValueManager;
class Value;
class ValueView;

namespace common_internal {

class LegacyMapValue;
class LegacyMapValueView;

bool Is(LegacyMapValueView lhs, LegacyMapValueView rhs);

class LegacyMapValue final {
 public:
  using view_alternative_type = LegacyMapValueView;

  static constexpr ValueKind kKind = ValueKind::kMap;

  explicit LegacyMapValue(LegacyMapValueView value);

  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit LegacyMapValue(uintptr_t impl) : impl_(impl) {}

  // By default, this creates an empty map whose type is `map(dyn, dyn)`.
  // Unless you can help it, you should use a more specific typed map value.
  LegacyMapValue();
  LegacyMapValue(const LegacyMapValue&) = default;
  LegacyMapValue(LegacyMapValue&&) = default;
  LegacyMapValue& operator=(const LegacyMapValue&) = default;
  LegacyMapValue& operator=(LegacyMapValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  MapType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const { return "map"; }

  std::string DebugString() const;

  // See `ValueInterface::GetSerializedSize`.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // See `ValueInterface::SerializeTo`.
  absl::Status SerializeTo(absl::Cord& value) const;

  // See `ValueInterface::Serialize`.
  absl::StatusOr<absl::Cord> Serialize() const;

  // See `ValueInterface::GetTypeUrl`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // See `ValueInterface::ConvertToAny`.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const { return ConvertToJsonObject(); }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return IsEmpty(); }

  bool IsEmpty() const;

  size_t Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Get(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<std::pair<ValueView, bool>> Find(
      ValueManager& value_manager, ValueView key,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> Has(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ListValueView> ListKeys(
      ValueManager& value_manager,
      ListValue& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  void swap(LegacyMapValue& other) noexcept {
    using std::swap;
    swap(impl_, other.impl_);
  }

  uintptr_t NativeValue() const { return impl_; }

 private:
  friend class LegacyMapValueView;

  uintptr_t impl_;
};

inline void swap(LegacyMapValue& lhs, LegacyMapValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, const LegacyMapValue& type) {
  return out << type.DebugString();
}

class LegacyMapValueView final {
 public:
  using alternative_type = LegacyMapValue;

  static constexpr ValueKind kKind = LegacyMapValue::kKind;

  explicit LegacyMapValueView(uintptr_t impl) : impl_(impl) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  LegacyMapValueView(
      const LegacyMapValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : impl_(value.impl_) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  LegacyMapValueView& operator=(
      const LegacyMapValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    impl_ = value.impl_;
    return *this;
  }

  LegacyMapValueView& operator=(LegacyMapValue&&) = delete;

  // By default, this creates an empty map whose type is `map(dyn, dyn)`. Unless
  // you can help it, you should use a more specific typed map value.
  LegacyMapValueView();
  LegacyMapValueView(const LegacyMapValueView&) = default;
  LegacyMapValueView(LegacyMapValueView&&) = default;
  LegacyMapValueView& operator=(const LegacyMapValueView&) = default;
  LegacyMapValueView& operator=(LegacyMapValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  MapType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const { return "map"; }

  std::string DebugString() const;

  // See `ValueInterface::GetSerializedSize`.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // See `ValueInterface::SerializeTo`.
  absl::Status SerializeTo(absl::Cord& value) const;

  // See `ValueInterface::Serialize`.
  absl::StatusOr<absl::Cord> Serialize() const;

  // See `ValueInterface::GetTypeUrl`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // See `ValueInterface::ConvertToAny`.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const { return ConvertToJsonObject(); }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return IsEmpty(); }

  bool IsEmpty() const;

  size_t Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Get(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<std::pair<ValueView, bool>> Find(
      ValueManager& value_manager, ValueView key,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> Has(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ListValueView> ListKeys(
      ValueManager& value_manager,
      ListValue& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  void swap(LegacyMapValueView& other) noexcept {
    using std::swap;
    swap(impl_, other.impl_);
  }

  uintptr_t NativeValue() const { return impl_; }

 private:
  friend class LegacyMapValue;
  friend bool Is(LegacyMapValueView lhs, LegacyMapValueView rhs);

  uintptr_t impl_;
};

inline void swap(LegacyMapValueView& lhs, LegacyMapValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, LegacyMapValueView type) {
  return out << type.DebugString();
}

inline LegacyMapValue::LegacyMapValue(LegacyMapValueView value)
    : impl_(value.impl_) {}

inline bool Is(LegacyMapValueView lhs, LegacyMapValueView rhs) {
  return lhs.impl_ == rhs.impl_;
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_MAP_VALUE_H_
