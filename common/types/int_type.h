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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_INT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_INT_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class IntType;
class IntTypeView;

// `IntType` represents the primitive `int` type.
class IntType final {
 public:
  using view_alternative_type = IntTypeView;

  static constexpr TypeKind kKind = TypeKind::kInt;
  static constexpr absl::string_view kName = "int";

  explicit IntType(IntTypeView);

  IntType() = default;
  IntType(const IntType&) = default;
  IntType(IntType&&) = default;
  IntType& operator=(const IntType&) = default;
  IntType& operator=(IntType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(IntType&) noexcept {}
};

inline constexpr void swap(IntType& lhs, IntType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(IntType, IntType) { return true; }

inline constexpr bool operator!=(IntType lhs, IntType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, IntType) {
  // IntType is really a singleton and all instances are equal. Nothing to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const IntType& type) {
  return out << type.DebugString();
}

class IntTypeView final {
 public:
  using alternative_type = IntType;

  static constexpr TypeKind kKind = IntType::kKind;
  static constexpr absl::string_view kName = IntType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  IntTypeView(const IntType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                  ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  IntTypeView& operator=(
      const IntType& type ABSL_ATTRIBUTE_LIFETIME_BOUND ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  IntTypeView& operator=(IntType&&) = delete;

  IntTypeView() = default;
  IntTypeView(const IntTypeView&) = default;
  IntTypeView(IntTypeView&&) = default;
  IntTypeView& operator=(const IntTypeView&) = default;
  IntTypeView& operator=(IntTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(IntTypeView&) noexcept {}
};

inline constexpr void swap(IntTypeView& lhs, IntTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(IntTypeView, IntTypeView) { return true; }

inline constexpr bool operator!=(IntTypeView lhs, IntTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, IntTypeView type) {
  // IntType is really a singleton and all instances are equal. Nothing to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, IntTypeView type) {
  return out << type.DebugString();
}

inline IntType::IntType(IntTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_INT_TYPE_H_
