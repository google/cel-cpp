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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class EnumValue;
class EnumValueView;

class EnumValue final {
 public:
  using view_alternative_type = EnumValueView;

  static constexpr ValueKind kKind = ValueKind::kEnum;

  explicit EnumValue(EnumValueView value);

  EnumValue(EnumType type, EnumTypeValueId id)
      : type_(std::move(type)), id_(id) {}

  EnumValue() = delete;
  EnumValue(const EnumValue&) = default;
  EnumValue(EnumValue&&) = default;
  EnumValue& operator=(const EnumValue&) = default;
  EnumValue& operator=(EnumValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  EnumTypeView type() const { return type_; }

  std::string DebugString() const;

  int64_t number() const { return type_.GetNumberForId(id_); }

  absl::string_view name() const { return type_.GetNameForId(id_); }

  void swap(EnumValue& other) noexcept {
    using std::swap;
    swap(type_, other.type_);
    swap(id_, other.id_);
  }

 private:
  friend class EnumValueView;
  friend class EnumTypeInterface;
  friend class EnumTypeValueIterator;

  EnumType type_;
  EnumTypeValueId id_;
};

inline void swap(EnumValue& lhs, EnumValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const EnumValue& value) {
  return out << value.DebugString();
}

class EnumValueView final {
 public:
  using alternative_type = EnumValue;

  static constexpr ValueKind kKind = EnumValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  EnumValueView(const EnumValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : type_(value.type_), id_(value.id_) {}

  EnumValueView(EnumTypeView type, EnumTypeValueId id) : type_(type), id_(id) {}

  EnumValueView() = delete;
  EnumValueView(const EnumValueView&) = default;
  EnumValueView(EnumValueView&&) = default;
  EnumValueView& operator=(const EnumValueView&) = default;
  EnumValueView& operator=(EnumValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  EnumTypeView type() const { return type_; }

  std::string DebugString() const;

  int64_t number() const { return type_.GetNumberForId(id_); }

  absl::string_view name() const { return type_.GetNameForId(id_); }

  void swap(EnumValueView& other) noexcept {
    using std::swap;
    swap(type_, other.type_);
    swap(id_, other.id_);
  }

 private:
  friend class EnumValue;

  EnumTypeView type_;
  EnumTypeValueId id_;
};

inline void swap(EnumValueView& lhs, EnumValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, EnumValueView value) {
  return out << value.DebugString();
}

inline EnumValue::EnumValue(EnumValueView value)
    : type_(value.type_), id_(value.id_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_
