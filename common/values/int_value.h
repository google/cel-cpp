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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_

#include <cstddef>
#include <cstdint>
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

class IntValue;
class IntValueView;
class TypeManager;

// `IntValue` represents values of the primitive `int` type.
class IntValue final {
 public:
  using view_alternative_type = IntValueView;

  static constexpr ValueKind kKind = ValueKind::kInt;

  constexpr explicit IntValue(int64_t value) noexcept : value_(value) {}

  constexpr explicit IntValue(IntValueView value) noexcept;

  IntValue() = default;
  IntValue(const IntValue&) = default;
  IntValue(IntValue&&) = default;
  IntValue& operator=(const IntValue&) = default;
  IntValue& operator=(IntValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  IntType GetType(TypeManager&) const { return IntType(); }

  absl::string_view GetTypeName() const { return IntType::kName; }

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

  constexpr int64_t NativeValue() const { return value_; }

  void swap(IntValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class IntValueView;

  int64_t value_ = 0;
};

inline void swap(IntValue& lhs, IntValue& rhs) noexcept { lhs.swap(rhs); }

template <typename H>
H AbslHashValue(H state, IntValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(IntValue lhs, IntValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(IntValue lhs, int64_t rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(int64_t lhs, IntValue rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator!=(IntValue lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(IntValue lhs, int64_t rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(int64_t lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator<(IntValue lhs, IntValue rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

constexpr bool operator<(IntValue lhs, int64_t rhs) {
  return lhs.NativeValue() < rhs;
}

constexpr bool operator<(int64_t lhs, IntValue rhs) {
  return lhs < rhs.NativeValue();
}

inline std::ostream& operator<<(std::ostream& out, IntValue value) {
  return out << value.DebugString();
}

class IntValueView final {
 public:
  using alternative_type = IntValue;

  static constexpr ValueKind kKind = IntValue::kKind;

  constexpr explicit IntValueView(int64_t value) noexcept : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr IntValueView(IntValue value) noexcept
      : IntValueView(value.value_) {}

  IntValueView() = default;
  IntValueView(const IntValueView&) = default;
  IntValueView(IntValueView&&) = default;
  IntValueView& operator=(const IntValueView&) = default;
  IntValueView& operator=(IntValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  IntType GetType(TypeManager&) const { return IntType(); }

  absl::string_view GetTypeName() const { return IntType::kName; }

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

  constexpr int64_t NativeValue() const { return value_; }

  void swap(IntValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class IntValue;

  // We pass around by value, as its cheaper than passing around a pointer due
  // to the performance degradation from pointer chasing.
  int64_t value_ = 0;
};

inline void swap(IntValueView& lhs, IntValueView& rhs) noexcept {
  lhs.swap(rhs);
}

template <typename H>
H AbslHashValue(H state, IntValueView value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(IntValueView lhs, IntValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(IntValueView lhs, int64_t rhs) {
  return lhs.NativeValue() == rhs;
}

constexpr bool operator==(int64_t lhs, IntValueView rhs) {
  return lhs == rhs.NativeValue();
}

constexpr bool operator==(IntValueView lhs, IntValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator==(IntValue lhs, IntValueView rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(IntValueView lhs, IntValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(IntValueView lhs, int64_t rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(int64_t lhs, IntValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(IntValueView lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(IntValue lhs, IntValueView rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator<(IntValueView lhs, IntValueView rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

constexpr bool operator<(IntValueView lhs, int64_t rhs) {
  return lhs.NativeValue() < rhs;
}

constexpr bool operator<(int64_t lhs, IntValueView rhs) {
  return lhs < rhs.NativeValue();
}

constexpr bool operator<(IntValueView lhs, IntValue rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

constexpr bool operator<(IntValue lhs, IntValueView rhs) {
  return lhs.NativeValue() < rhs.NativeValue();
}

inline std::ostream& operator<<(std::ostream& out, IntValueView value) {
  return out << value.DebugString();
}

inline constexpr IntValue::IntValue(IntValueView value) noexcept
    : IntValue(value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
