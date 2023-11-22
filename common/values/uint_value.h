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

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class UintValue;
class UintValueView;

// `UintValue` represents values of the primitive `uint` type.
class UintValue final {
 public:
  using view_alternative_type = UintValueView;

  static constexpr ValueKind kKind = ValueKind::kUint;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr UintValue(uint64_t value) noexcept : value_(value) {}

  explicit UintValue(UintValueView value) noexcept;

  UintValue() = default;
  UintValue(const UintValue&) = default;
  UintValue(UintValue&&) = default;
  UintValue& operator=(const UintValue&) = default;
  UintValue& operator=(UintValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  UintTypeView type() const { return UintTypeView(); }

  std::string DebugString() const;

  uint64_t NativeValue() const { return value_; }

  void swap(UintValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class UintValueView;

  uint64_t value_ = 0;
};

inline void swap(UintValue& lhs, UintValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const UintValue& value) {
  return out << value.DebugString();
}

class UintValueView final {
 private:
  static constexpr UintValue kZero{0};

 public:
  using alternative_type = UintValue;

  static constexpr ValueKind kKind = UintValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  UintValueView(
      const uint64_t&  // NOLINT(google3-readability-pass-trivial-by-value)
          value) noexcept
      : value_(std::addressof(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  UintValueView(const UintValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : UintValueView(value.value_) {}

  UintValueView() = default;
  UintValueView(const UintValueView&) = default;
  UintValueView(UintValueView&&) = default;
  UintValueView& operator=(const UintValueView&) = default;
  UintValueView& operator=(UintValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  UintTypeView type() const { return UintTypeView(); }

  std::string DebugString() const;

  uint64_t NativeValue() const { return *value_; }

  void swap(UintValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class UintValue;

  absl::Nonnull<const uint64_t*> value_ = std::addressof(kZero.value_);
};

inline void swap(UintValueView& lhs, UintValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, UintValueView value) {
  return out << value.DebugString();
}

inline UintValue::UintValue(UintValueView value) noexcept
    : value_(*value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_UINT_VALUE_H_
