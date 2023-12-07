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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_UINT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_UINT_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class UintType;
class UintTypeView;

// `UintType` represents the primitive `uint` type.
class UintType final {
 public:
  using view_alternative_type = UintTypeView;

  static constexpr TypeKind kKind = TypeKind::kUint;
  static constexpr absl::string_view kName = "uint";

  explicit UintType(UintTypeView);

  UintType() = default;
  UintType(const UintType&) = default;
  UintType(UintType&&) = default;
  UintType& operator=(const UintType&) = default;
  UintType& operator=(UintType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(UintType&) noexcept {}
};

inline constexpr void swap(UintType& lhs, UintType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(UintType, UintType) { return true; }

inline constexpr bool operator!=(UintType lhs, UintType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, UintType) {
  // UintType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const UintType& type) {
  return out << type.DebugString();
}

class UintTypeView final {
 public:
  using alternative_type = UintType;

  static constexpr TypeKind kKind = UintType::kKind;
  static constexpr absl::string_view kName = UintType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  UintTypeView(const UintType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                   ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  UintTypeView& operator=(const UintType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                              ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  UintTypeView& operator=(UintType&&) = delete;

  UintTypeView() = default;
  UintTypeView(const UintTypeView&) = default;
  UintTypeView(UintTypeView&&) = default;
  UintTypeView& operator=(const UintTypeView&) = default;
  UintTypeView& operator=(UintTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(UintTypeView&) noexcept {}
};

inline constexpr void swap(UintTypeView& lhs, UintTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(UintTypeView, UintTypeView) { return true; }

inline constexpr bool operator!=(UintTypeView lhs, UintTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, UintTypeView) {
  // UintType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, UintTypeView type) {
  return out << type.DebugString();
}

inline UintType::UintType(UintTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_UINT_TYPE_H_
