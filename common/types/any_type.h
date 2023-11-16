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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_ANY_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_ANY_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class AnyType;
class AnyTypeView;

// `AnyType` is a special type which has no direct value representation. It is
// used to represent `google.protobuf.Any`, which never exists at runtime as
// a value. Its primary usage is for type checking and unpacking at runtime.
class AnyType final {
 public:
  using view_alternative_type = AnyTypeView;

  static constexpr TypeKind kKind = TypeKind::kAny;
  static constexpr absl::string_view kName = "google.protobuf.Any";

  explicit AnyType(AnyTypeView);

  AnyType() = default;
  AnyType(const AnyType&) = default;
  AnyType(AnyType&&) = default;
  AnyType& operator=(const AnyType&) = default;
  AnyType& operator=(AnyType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(AnyType&) noexcept {}
};

inline constexpr void swap(AnyType& lhs, AnyType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(const AnyType&, const AnyType&) {
  return true;
}

inline constexpr bool operator!=(const AnyType& lhs, const AnyType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const AnyType& type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, const AnyType& type) {
  return out << type.DebugString();
}

class AnyTypeView final {
 public:
  using alternative_type = AnyType;

  static constexpr TypeKind kKind = AnyType::kKind;
  static constexpr absl::string_view kName = AnyType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  AnyTypeView(const AnyType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                  ABSL_ATTRIBUTE_UNUSED) noexcept {}

  AnyTypeView() = default;
  AnyTypeView(const AnyTypeView&) = default;
  AnyTypeView(AnyTypeView&&) = default;
  AnyTypeView& operator=(const AnyTypeView&) = default;
  AnyTypeView& operator=(AnyTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(AnyTypeView&) noexcept {}
};

inline constexpr void swap(AnyTypeView& lhs, AnyTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(AnyTypeView, AnyTypeView) { return true; }

inline constexpr bool operator!=(AnyTypeView lhs, AnyTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, AnyTypeView type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, AnyTypeView type) {
  return out << type.DebugString();
}

inline AnyType::AnyType(AnyTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_ANY_TYPE_H_
