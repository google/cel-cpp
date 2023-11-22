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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_

#include <memory>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class DoubleValue;
class DoubleValueView;

// `DoubleValue` represents values of the primitive `double` type.
class DoubleValue final {
 public:
  using view_alternative_type = DoubleValueView;

  static constexpr ValueKind kKind = ValueKind::kDouble;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DoubleValue(double value) noexcept : value_(value) {}

  explicit DoubleValue(DoubleValueView value) noexcept;

  DoubleValue() = default;
  DoubleValue(const DoubleValue&) = default;
  DoubleValue(DoubleValue&&) = default;
  DoubleValue& operator=(const DoubleValue&) = default;
  DoubleValue& operator=(DoubleValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DoubleTypeView type() const { return DoubleTypeView(); }

  std::string DebugString() const;

  double NativeValue() const { return value_; }

  void swap(DoubleValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DoubleValueView;

  double value_ = 0.0;
};

inline void swap(DoubleValue& lhs, DoubleValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const DoubleValue& value) {
  return out << value.DebugString();
}

class DoubleValueView final {
 private:
  static constexpr DoubleValue kZero{0.0};

 public:
  using alternative_type = DoubleValue;

  static constexpr ValueKind kKind = DoubleValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  DoubleValueView(
      const double&  // NOLINT(google3-readability-pass-trivial-by-value)
          value) noexcept
      : value_(std::addressof(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  DoubleValueView(
      const DoubleValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : DoubleValueView(value.value_) {}

  DoubleValueView() = default;
  DoubleValueView(const DoubleValueView&) = default;
  DoubleValueView(DoubleValueView&&) = default;
  DoubleValueView& operator=(const DoubleValueView&) = default;
  DoubleValueView& operator=(DoubleValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DoubleTypeView type() const { return DoubleTypeView(); }

  std::string DebugString() const;

  double NativeValue() const { return *value_; }

  void swap(DoubleValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DoubleValue;

  absl::Nonnull<const double*> value_ = std::addressof(kZero.value_);
};

inline void swap(DoubleValueView& lhs, DoubleValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, DoubleValueView value) {
  return out << value.DebugString();
}

inline DoubleValue::DoubleValue(DoubleValueView value) noexcept
    : value_(*value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_
