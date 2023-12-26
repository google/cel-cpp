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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class BoolValue;
class BoolValueView;
class TypeManager;

// `BoolValue` represents values of the primitive `bool` type.
class BoolValue final {
 public:
  using view_alternative_type = BoolValueView;

  static constexpr ValueKind kKind = ValueKind::kBool;

  constexpr explicit BoolValue(bool value) noexcept : value_(value) {}

  constexpr explicit BoolValue(BoolValueView value) noexcept;

  BoolValue() = default;
  BoolValue(const BoolValue&) = default;
  BoolValue(BoolValue&&) = default;
  BoolValue& operator=(const BoolValue&) = default;
  BoolValue& operator=(BoolValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  BoolType GetType(TypeManager&) const { return BoolType(); }

  absl::string_view GetTypeName() const { return BoolType::kName; }

  std::string DebugString() const;

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. This always
  // succeeds and only returns `absl::StatusOr` to meet concept requirements.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` serializes this value and returns it as `absl::Cord`.
  absl::StatusOr<absl::Cord> Serialize() const;

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // 'ConvertToAny' converts this value to `Any`.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  constexpr bool NativeValue() const { return value_; }

  void swap(BoolValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class BoolValueView;

  bool value_ = false;
};

inline void swap(BoolValue& lhs, BoolValue& rhs) noexcept { lhs.swap(rhs); }

template <typename H>
H AbslHashValue(H state, BoolValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(BoolValue lhs, BoolValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(BoolValue lhs, bool rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(bool lhs, BoolValue rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator!=(BoolValue lhs, BoolValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(BoolValue lhs, bool rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(bool lhs, BoolValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator<(BoolValue lhs, BoolValue rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

constexpr bool operator<(BoolValue lhs, bool rhs) {
  return lhs.NativeValue() < rhs;
}

constexpr bool operator<(bool lhs, BoolValue rhs) {
  return lhs < rhs.NativeValue();
}

inline std::ostream& operator<<(std::ostream& out, BoolValue value) {
  return out << value.DebugString();
}

class BoolValueView final {
 public:
  using alternative_type = BoolValue;

  static constexpr ValueKind kKind = BoolValue::kKind;

  constexpr explicit BoolValueView(bool value) noexcept : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr BoolValueView(BoolValue value) noexcept
      : BoolValueView(value.value_) {}

  BoolValueView() = default;
  BoolValueView(const BoolValueView&) = default;
  BoolValueView(BoolValueView&&) = default;
  BoolValueView& operator=(const BoolValueView&) = default;
  BoolValueView& operator=(BoolValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  BoolType GetType(TypeManager&) const { return BoolType(); }

  absl::string_view GetTypeName() const { return BoolType::kName; }

  std::string DebugString() const;

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. This always
  // succeeds and only returns `absl::StatusOr` to meet concept requirements.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` serializes this value and returns it as `absl::Cord`.
  absl::StatusOr<absl::Cord> Serialize() const;

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // 'ConvertToAny' converts this value to `Any`.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson() const;

  constexpr bool NativeValue() const { return value_; }

  void swap(BoolValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class BoolValue;

  // We pass around by value, as its cheaper than passing around a pointer both
  // in size and the performance degradation from pointer chasing.
  bool value_ = false;
};

inline void swap(BoolValueView& lhs, BoolValueView& rhs) noexcept {
  lhs.swap(rhs);
}

template <typename H>
H AbslHashValue(H state, BoolValueView value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(BoolValueView lhs, BoolValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(BoolValueView lhs, bool rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(bool lhs, BoolValueView rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator==(BoolValueView lhs, BoolValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(BoolValue lhs, BoolValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(BoolValueView lhs, BoolValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(BoolValueView lhs, bool rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(bool lhs, BoolValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(BoolValueView lhs, BoolValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(BoolValue lhs, BoolValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator<(BoolValueView lhs, BoolValueView rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

constexpr bool operator<(BoolValueView lhs, bool rhs) {
  return lhs.NativeValue() < rhs;
}

constexpr bool operator<(bool lhs, BoolValueView rhs) {
  return lhs < rhs.NativeValue();
}

constexpr bool operator<(BoolValueView lhs, BoolValue rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

constexpr bool operator<(BoolValue lhs, BoolValueView rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

inline std::ostream& operator<<(std::ostream& out, BoolValueView value) {
  return out << value.DebugString();
}

inline constexpr BoolValue::BoolValue(BoolValueView value) noexcept
    : BoolValue(value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
