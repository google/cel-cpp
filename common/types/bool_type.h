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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_BOOL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_BOOL_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class BoolType;
class BoolTypeView;

// `BoolType` represents the primitive `bool` type.
class BoolType final {
 public:
  using view_alternative_type = BoolTypeView;

  static constexpr TypeKind kKind = TypeKind::kBool;
  static constexpr absl::string_view kName = "bool";

  explicit BoolType(BoolTypeView);

  BoolType() = default;
  BoolType(const BoolType&) = default;
  BoolType(BoolType&&) = default;
  BoolType& operator=(const BoolType&) = default;
  BoolType& operator=(BoolType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BoolType&) noexcept {}
};

inline constexpr void swap(BoolType& lhs, BoolType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(const BoolType&, const BoolType&) {
  return true;
}

inline constexpr bool operator!=(const BoolType& lhs, const BoolType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const BoolType& type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, const BoolType& type) {
  return out << type.DebugString();
}

class BoolTypeView final {
 public:
  using alternative_type = BoolType;

  static constexpr TypeKind kKind = BoolType::kKind;
  static constexpr absl::string_view kName = BoolType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  BoolTypeView(const BoolType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                   ABSL_ATTRIBUTE_UNUSED) noexcept {}

  BoolTypeView() = default;
  BoolTypeView(const BoolTypeView&) = default;
  BoolTypeView(BoolTypeView&&) = default;
  BoolTypeView& operator=(const BoolTypeView&) = default;
  BoolTypeView& operator=(BoolTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BoolTypeView&) noexcept {}
};

inline constexpr void swap(BoolTypeView& lhs, BoolTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(BoolTypeView, BoolTypeView) { return true; }

inline constexpr bool operator!=(BoolTypeView lhs, BoolTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BoolTypeView type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, BoolTypeView type) {
  return out << type.DebugString();
}

inline BoolType::BoolType(BoolTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_BOOL_TYPE_H_
