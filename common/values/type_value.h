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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_

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
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class TypeValue;
class TypeValueView;

// `TypeValue` represents values of the primitive `type` type.
class TypeValue final {
 public:
  using view_alternative_type = TypeValueView;

  static constexpr ValueKind kKind = ValueKind::kType;

  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeValue(Type value) noexcept : value_(std::move(value)) {}

  explicit TypeValue(TypeValueView value) noexcept;

  TypeValue() = delete;
  TypeValue(const TypeValue&) = default;
  TypeValue(TypeValue&&) = default;
  TypeValue& operator=(const TypeValue&) = default;
  TypeValue& operator=(TypeValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  TypeTypeView type() const { return TypeTypeView(); }

  std::string DebugString() const { return value_.DebugString(); }

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `TypeValue` is
  // not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  TypeView NativeValue() const { return value_; }

  void swap(TypeValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class TypeValueView;
  friend struct NativeTypeTraits<TypeValue>;

  Type value_;
};

inline void swap(TypeValue& lhs, TypeValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const TypeValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<TypeValue> final {
  static bool SkipDestructor(const TypeValue& value) {
    return NativeTypeTraits<Type>::SkipDestructor(value.value_);
  }
};

class TypeValueView final {
 public:
  using alternative_type = TypeValue;

  static constexpr ValueKind kKind = TypeValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeValueView(TypeView value) noexcept : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeValueView(const TypeValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : TypeValueView(TypeView(value.value_)) {}

  TypeValueView() = delete;
  TypeValueView(const TypeValueView&) = default;
  TypeValueView(TypeValueView&&) = default;
  TypeValueView& operator=(const TypeValueView&) = default;
  TypeValueView& operator=(TypeValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  TypeTypeView type() const { return TypeTypeView(); }

  std::string DebugString() const { return value_.DebugString(); }

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `TypeValue` is
  // not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  TypeView NativeValue() const { return value_; }

  void swap(TypeValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class TypeValue;

  TypeView value_;
};

inline void swap(TypeValueView& lhs, TypeValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, TypeValueView value) {
  return out << value.DebugString();
}

inline TypeValue::TypeValue(TypeValueView value) noexcept
    : value_(value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_
