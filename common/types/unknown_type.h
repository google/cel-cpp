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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_UNKNOWN_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_UNKNOWN_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class UnknownType;
class UnknownTypeView;

// `UnknownType` is a special type which represents an unknown at runtime. It
// has no in-language representation.
class UnknownType final {
 public:
  using view_alternative_type = UnknownTypeView;

  static constexpr TypeKind kKind = TypeKind::kUnknown;
  static constexpr absl::string_view kName = "*unknown*";

  explicit UnknownType(UnknownTypeView);

  UnknownType() = default;
  UnknownType(const UnknownType&) = default;
  UnknownType(UnknownType&&) = default;
  UnknownType& operator=(const UnknownType&) = default;
  UnknownType& operator=(UnknownType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kName;
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {};
  }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(UnknownType&) noexcept {}
};

inline constexpr void swap(UnknownType& lhs, UnknownType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(UnknownType, UnknownType) { return true; }

inline constexpr bool operator!=(UnknownType lhs, UnknownType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, UnknownType) {
  // UnknownType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const UnknownType& type) {
  return out << type.DebugString();
}

class UnknownTypeView final {
 public:
  using alternative_type = UnknownType;

  static constexpr TypeKind kKind = UnknownType::kKind;
  static constexpr absl::string_view kName = UnknownType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  UnknownTypeView(const UnknownType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                      ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  UnknownTypeView& operator=(
      const UnknownType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  UnknownTypeView& operator=(UnknownType&&) = delete;

  UnknownTypeView() = default;
  UnknownTypeView(const UnknownTypeView&) = default;
  UnknownTypeView(UnknownTypeView&&) = default;
  UnknownTypeView& operator=(const UnknownTypeView&) = default;
  UnknownTypeView& operator=(UnknownTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(UnknownTypeView&) noexcept {}
};

inline constexpr void swap(UnknownTypeView& lhs,
                           UnknownTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(UnknownTypeView, UnknownTypeView) {
  return true;
}

inline constexpr bool operator!=(UnknownTypeView lhs, UnknownTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, UnknownTypeView type) {
  // UnknownType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, UnknownTypeView type) {
  return out << type.DebugString();
}

inline UnknownType::UnknownType(UnknownTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_UNKNOWN_TYPE_H_
