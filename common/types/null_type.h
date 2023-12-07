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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_NULL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_NULL_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class NullType;
class NullTypeView;

// `NullType` represents the primitive `null_type` type.
class NullType final {
 public:
  using view_alternative_type = NullTypeView;

  static constexpr TypeKind kKind = TypeKind::kNull;
  static constexpr absl::string_view kName = "null_type";

  explicit NullType(NullTypeView);

  NullType() = default;
  NullType(const NullType&) = default;
  NullType(NullType&&) = default;
  NullType& operator=(const NullType&) = default;
  NullType& operator=(NullType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(NullType&) noexcept {}
};

inline constexpr void swap(NullType& lhs, NullType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(NullType, NullType) { return true; }

inline constexpr bool operator!=(NullType lhs, NullType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, NullType) {
  // NullType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const NullType& type) {
  return out << type.DebugString();
}

class NullTypeView final {
 public:
  using alternative_type = NullType;

  static constexpr TypeKind kKind = NullType::kKind;
  static constexpr absl::string_view kName = NullType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  NullTypeView(const NullType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                   ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  NullTypeView& operator=(const NullType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                              ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  NullTypeView& operator=(NullType&&) = delete;

  NullTypeView() = default;
  NullTypeView(const NullTypeView&) = default;
  NullTypeView(NullTypeView&&) = default;
  NullTypeView& operator=(const NullTypeView&) = default;
  NullTypeView& operator=(NullTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(NullTypeView&) noexcept {}
};

inline constexpr void swap(NullTypeView& lhs, NullTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(NullTypeView, NullTypeView) { return true; }

inline constexpr bool operator!=(NullTypeView lhs, NullTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, NullTypeView) {
  // NullType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, NullTypeView type) {
  return out << type.DebugString();
}

inline NullType::NullType(NullTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_NULL_TYPE_H_
