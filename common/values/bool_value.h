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

#include <memory>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class BoolValue;
class BoolValueView;

// `BoolValue` represents values of the primitive `bool` type.
class BoolValue final {
 public:
  using view_alternative_type = BoolValueView;

  static constexpr ValueKind kKind = ValueKind::kBool;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr BoolValue(bool value) noexcept : value_(value) {}

  explicit BoolValue(BoolValueView value) noexcept;

  BoolValue() = default;
  BoolValue(const BoolValue&) = default;
  BoolValue(BoolValue&&) = default;
  BoolValue& operator=(const BoolValue&) = default;
  BoolValue& operator=(BoolValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  BoolTypeView type() const { return BoolTypeView(); }

  std::string DebugString() const;

  bool NativeValue() const { return value_; }

  void swap(BoolValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class BoolValueView;

  bool value_ = false;
};

inline void swap(BoolValue& lhs, BoolValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const BoolValue& value) {
  return out << value.DebugString();
}

class BoolValueView final {
 private:
  static constexpr BoolValue kFalse{false};
  static constexpr BoolValue kTrue{true};

 public:
  using alternative_type = BoolValue;

  static constexpr ValueKind kKind = BoolValue::kKind;

  explicit BoolValueView(bool value) noexcept
      : BoolValueView(value ? kTrue : kFalse) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  BoolValueView(const BoolValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : value_(std::addressof(value.value_)) {}

  BoolValueView() = default;
  BoolValueView(const BoolValueView&) = default;
  BoolValueView(BoolValueView&&) = default;
  BoolValueView& operator=(const BoolValueView&) = default;
  BoolValueView& operator=(BoolValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  BoolTypeView type() const { return BoolTypeView(); }

  std::string DebugString() const;

  bool NativeValue() const { return *value_; }

  void swap(BoolValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class BoolValue;

  absl::Nonnull<const bool*> value_ = std::addressof(kFalse.value_);
};

inline void swap(BoolValueView& lhs, BoolValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, BoolValueView value) {
  return out << value.DebugString();
}

inline BoolValue::BoolValue(BoolValueView value) noexcept
    : value_(*value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
