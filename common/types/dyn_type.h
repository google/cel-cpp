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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_DYN_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_DYN_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class DynType;
class DynTypeView;

// `DynType` is a special type which represents any type and has no direct value
// representation.
class DynType final {
 public:
  using view_alternative_type = DynTypeView;

  static constexpr TypeKind kKind = TypeKind::kDyn;
  static constexpr absl::string_view kName = "dyn";

  explicit DynType(DynTypeView);

  DynType() = default;
  DynType(const DynType&) = default;
  DynType(DynType&&) = default;
  DynType& operator=(const DynType&) = default;
  DynType& operator=(DynType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(DynType&) noexcept {}
};

inline constexpr void swap(DynType& lhs, DynType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(const DynType&, const DynType&) {
  return true;
}

inline constexpr bool operator!=(const DynType& lhs, const DynType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const DynType& type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, const DynType& type) {
  return out << type.DebugString();
}

class DynTypeView final {
 public:
  using alternative_type = DynType;

  static constexpr TypeKind kKind = DynType::kKind;
  static constexpr absl::string_view kName = DynType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  DynTypeView(const DynType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                  ABSL_ATTRIBUTE_UNUSED) noexcept {}

  DynTypeView() = default;
  DynTypeView(const DynTypeView&) = default;
  DynTypeView(DynTypeView&&) = default;
  DynTypeView& operator=(const DynTypeView&) = default;
  DynTypeView& operator=(DynTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(DynTypeView&) noexcept {}
};

inline constexpr void swap(DynTypeView& lhs, DynTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(DynTypeView, DynTypeView) { return true; }

inline constexpr bool operator!=(DynTypeView lhs, DynTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, DynTypeView type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, DynTypeView type) {
  return out << type.DebugString();
}

inline DynType::DynType(DynTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_DYN_TYPE_H_
