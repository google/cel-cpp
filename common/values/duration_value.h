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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_

#include <memory>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/time/time.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class DurationValue;
class DurationValueView;

// `DurationValue` represents values of the primitive `duration` type.
class DurationValue final {
 public:
  using view_alternative_type = DurationValueView;

  static constexpr ValueKind kKind = ValueKind::kDuration;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DurationValue(absl::Duration value) noexcept : value_(value) {}

  explicit DurationValue(DurationValueView value) noexcept;

  DurationValue() = default;
  DurationValue(const DurationValue&) = default;
  DurationValue(DurationValue&&) = default;
  DurationValue& operator=(const DurationValue&) = default;
  DurationValue& operator=(DurationValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DurationTypeView type() const { return DurationTypeView(); }

  std::string DebugString() const;

  absl::Duration NativeValue() const { return value_; }

  void swap(DurationValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DurationValueView;

  absl::Duration value_ = absl::ZeroDuration();
};

inline void swap(DurationValue& lhs, DurationValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, const DurationValue& value) {
  return out << value.DebugString();
}

class DurationValueView final {
 private:
  static constexpr DurationValue kZero{absl::ZeroDuration()};

 public:
  using alternative_type = DurationValue;

  static constexpr ValueKind kKind = DurationValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  DurationValueView(
      const absl::
          Duration&  // NOLINT(google3-readability-pass-trivial-by-value)
              value) noexcept
      : value_(std::addressof(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  DurationValueView(
      const DurationValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : DurationValueView(value.value_) {}

  DurationValueView() = default;
  DurationValueView(const DurationValueView&) = default;
  DurationValueView(DurationValueView&&) = default;
  DurationValueView& operator=(const DurationValueView&) = default;
  DurationValueView& operator=(DurationValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  DurationTypeView type() const { return DurationTypeView(); }

  std::string DebugString() const;

  absl::Duration NativeValue() const { return *value_; }

  void swap(DurationValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

 private:
  friend class DurationValue;

  absl::Nonnull<const absl::Duration*> value_ = std::addressof(kZero.value_);
};

inline void swap(DurationValueView& lhs, DurationValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValueView value) {
  return out << value.DebugString();
}

inline DurationValue::DurationValue(DurationValueView value) noexcept
    : value_(*value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
