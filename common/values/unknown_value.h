// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/unknown.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ValueView;
class ValueManager;
class UnknownValue;
class UnknownValueView;
class TypeManager;

// `UnknownValue` represents values of the primitive `duration` type.
class UnknownValue final {
 public:
  using view_alternative_type = UnknownValueView;

  static constexpr ValueKind kKind = ValueKind::kUnknown;

  explicit UnknownValue(UnknownValueView) noexcept;

  explicit UnknownValue(Unknown unknown) : unknown_(std::move(unknown)) {}

  UnknownValue() = default;
  UnknownValue(const UnknownValue&) = default;
  UnknownValue(UnknownValue&&) = default;
  UnknownValue& operator=(const UnknownValue&) = default;
  UnknownValue& operator=(UnknownValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  UnknownType GetType(TypeManager&) const { return UnknownType(); }

  absl::string_view GetTypeName() const { return UnknownType::kName; }

  std::string DebugString() const { return ""; }

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `UnknownValue`
  // is not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `UnknownValue` is
  // not serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToJson` always returns `FAILED_PRECONDITION` as `UnknownValue` is
  // not convertible to JSON.
  absl::StatusOr<Json> ConvertToJson() const;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return false; }

  void swap(UnknownValue& other) noexcept {
    using std::swap;
    swap(unknown_, other.unknown_);
  }

  const Unknown& NativeValue() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return unknown_;
  }

  Unknown NativeValue() && {
    Unknown unknown = std::move(unknown_);
    return unknown;
  }

 private:
  friend class UnknownValueView;

  Unknown unknown_;
};

inline void swap(UnknownValue& lhs, UnknownValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, const UnknownValue& value) {
  return out << value.DebugString();
}

class UnknownValueView final {
 private:
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Unknown& Empty();

 public:
  using alternative_type = UnknownValue;

  static constexpr ValueKind kKind = UnknownValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  UnknownValueView(
      const UnknownValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : unknown_(&value.unknown_) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  UnknownValueView(
      const Unknown& unknown ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : unknown_(&unknown) {}

  UnknownValueView(UnknownValue&&) = delete;

  UnknownValueView(Unknown&&) = delete;

  // NOLINTNEXTLINE(google-explicit-constructor)
  UnknownValueView& operator=(
      const UnknownValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    unknown_ = &value.unknown_;
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  UnknownValueView& operator=(
      const Unknown& unknown ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    unknown_ = &unknown;
    return *this;
  }

  UnknownValueView& operator=(UnknownValue&&) = delete;

  UnknownValueView& operator=(Unknown&&) = delete;

  UnknownValueView() = default;
  UnknownValueView(const UnknownValueView&) = default;
  UnknownValueView& operator=(const UnknownValueView&) = default;

  constexpr ValueKind kind() const { return kKind; }

  UnknownType GetType(TypeManager&) const { return UnknownType(); }

  absl::string_view GetTypeName() const { return UnknownType::kName; }

  std::string DebugString() const { return ""; }

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `UnknownValue`
  // is not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `UnknownValue` is
  // not serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToJson` always returns `FAILED_PRECONDITION` as `UnknownValue` is
  // not convertible to JSON.
  absl::StatusOr<Json> ConvertToJson() const;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return false; }

  const Unknown& NativeValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *unknown_;
  }

  void swap(UnknownValueView& other) noexcept {
    using std::swap;
    swap(unknown_, other.unknown_);
  }

 private:
  friend class UnknownValue;

  const Unknown* unknown_ = &Empty();
};

inline void swap(UnknownValueView& lhs, UnknownValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, UnknownValueView value) {
  return out << value.DebugString();
}

inline UnknownValue::UnknownValue(UnknownValueView value) noexcept
    : unknown_(*value.unknown_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_
