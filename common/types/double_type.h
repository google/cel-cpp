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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_DOUBLE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_DOUBLE_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class DoubleType;
class DoubleTypeView;

// `BoolType` represents the primitive `double` type.
class DoubleType final {
 public:
  using view_alternative_type = DoubleTypeView;

  static constexpr TypeKind kKind = TypeKind::kDouble;
  static constexpr absl::string_view kName = "double";

  explicit DoubleType(DoubleTypeView);

  DoubleType() = default;
  DoubleType(const DoubleType&) = default;
  DoubleType(DoubleType&&) = default;
  DoubleType& operator=(const DoubleType&) = default;
  DoubleType& operator=(DoubleType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kName;
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {};
  }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(DoubleType&) noexcept {}
};

inline constexpr void swap(DoubleType& lhs, DoubleType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(DoubleType, DoubleType) { return true; }

inline constexpr bool operator!=(DoubleType lhs, DoubleType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, DoubleType) {
  // DoubleType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const DoubleType& type) {
  return out << type.DebugString();
}

class DoubleTypeView final {
 public:
  using alternative_type = DoubleType;

  static constexpr TypeKind kKind = DoubleType::kKind;
  static constexpr absl::string_view kName = DoubleType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  DoubleTypeView(const DoubleType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                     ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  DoubleTypeView& operator=(const DoubleType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                                ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  DoubleTypeView& operator=(DoubleType&&) = delete;

  DoubleTypeView() = default;
  DoubleTypeView(const DoubleTypeView&) = default;
  DoubleTypeView(DoubleTypeView&&) = default;
  DoubleTypeView& operator=(const DoubleTypeView&) = default;
  DoubleTypeView& operator=(DoubleTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(DoubleTypeView&) noexcept {}
};

inline constexpr void swap(DoubleTypeView& lhs, DoubleTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(DoubleTypeView, DoubleTypeView) {
  return true;
}

inline constexpr bool operator!=(DoubleTypeView lhs, DoubleTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, DoubleTypeView) {
  // DoubleType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, DoubleTypeView type) {
  return out << type.DebugString();
}

inline DoubleType::DoubleType(DoubleTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_DOUBLE_TYPE_H_
