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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_INT_WRAPPER_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_INT_WRAPPER_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class IntWrapperType;
class IntWrapperTypeView;

// `IntWrapperType` is a special type which has no direct value
// representation. It is used to represent `google.protobuf.Int64Value`, which
// never exists at runtime as a value. Its primary usage is for type checking
// and unpacking at runtime.
class IntWrapperType final {
 public:
  using view_alternative_type = IntWrapperTypeView;

  static constexpr TypeKind kKind = TypeKind::kIntWrapper;
  static constexpr absl::string_view kName = "google.protobuf.Int64Value";

  explicit IntWrapperType(IntWrapperTypeView);

  IntWrapperType() = default;
  IntWrapperType(const IntWrapperType&) = default;
  IntWrapperType(IntWrapperType&&) = default;
  IntWrapperType& operator=(const IntWrapperType&) = default;
  IntWrapperType& operator=(IntWrapperType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(IntWrapperType&) noexcept {}
};

inline constexpr void swap(IntWrapperType& lhs, IntWrapperType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(IntWrapperType, IntWrapperType) {
  return true;
}

inline constexpr bool operator!=(IntWrapperType lhs, IntWrapperType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, IntWrapperType) {
  // IntWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const IntWrapperType& type) {
  return out << type.DebugString();
}

class IntWrapperTypeView final {
 public:
  using alternative_type = IntWrapperType;

  static constexpr TypeKind kKind = IntWrapperType::kKind;
  static constexpr absl::string_view kName = IntWrapperType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  IntWrapperTypeView(const IntWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                         ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  IntWrapperTypeView& operator=(
      const IntWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  IntWrapperTypeView& operator=(IntWrapperType&&) = delete;

  IntWrapperTypeView() = default;
  IntWrapperTypeView(const IntWrapperTypeView&) = default;
  IntWrapperTypeView(IntWrapperTypeView&&) = default;
  IntWrapperTypeView& operator=(const IntWrapperTypeView&) = default;
  IntWrapperTypeView& operator=(IntWrapperTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(IntWrapperTypeView&) noexcept {}
};

inline constexpr void swap(IntWrapperTypeView& lhs,
                           IntWrapperTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(IntWrapperTypeView, IntWrapperTypeView) {
  return true;
}

inline constexpr bool operator!=(IntWrapperTypeView lhs,
                                 IntWrapperTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, IntWrapperTypeView) {
  // IntWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, IntWrapperTypeView type) {
  return out << type.DebugString();
}

inline IntWrapperType::IntWrapperType(IntWrapperTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_INT_WRAPPER_TYPE_H_
