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

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class IntValue;
class IntValueView;

// `IntValue` represents values of the primitive `int` type.
class IntValue final {
 public:
  using view_alternative_type = IntValueView;

  static constexpr ValueKind kKind = ValueKind::kInt;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr IntValue(int64_t value) noexcept : value_(value) {}

  explicit IntValue(IntValueView value) noexcept;

  IntValue() = default;
  IntValue(const IntValue&) = default;
  IntValue(IntValue&&) = default;
  IntValue& operator=(const IntValue&) = default;
  IntValue& operator=(IntValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  IntTypeView type() const { return IntTypeView(); }

  std::string DebugString() const;

  int64_t NativeValue() const { return value_; }

  void swap(IntValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class IntValueView;

  int64_t value_ = 0;
};

inline void swap(IntValue& lhs, IntValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const IntValue& value) {
  return out << value.DebugString();
}

class IntValueView final {
 private:
  static constexpr IntValue kZero{0};

 public:
  using alternative_type = IntValue;

  static constexpr ValueKind kKind = IntValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  IntValueView(
      const int64_t&  // NOLINT(google3-readability-pass-trivial-by-value)
          value) noexcept
      : value_(std::addressof(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  IntValueView(const IntValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : IntValueView(value.value_) {}

  IntValueView() = default;
  IntValueView(const IntValueView&) = default;
  IntValueView(IntValueView&&) = default;
  IntValueView& operator=(const IntValueView&) = default;
  IntValueView& operator=(IntValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  IntTypeView type() const { return IntTypeView(); }

  std::string DebugString() const;

  int64_t NativeValue() const { return *value_; }

  void swap(IntValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class IntValue;

  absl::Nonnull<const int64_t*> value_ = std::addressof(kZero.value_);
};

inline void swap(IntValueView& lhs, IntValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, IntValueView value) {
  return out << value.DebugString();
}

inline IntValue::IntValue(IntValueView value) noexcept
    : value_(*value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
