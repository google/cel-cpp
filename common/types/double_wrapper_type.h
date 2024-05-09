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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_DOUBLE_WRAPPER_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_DOUBLE_WRAPPER_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class DoubleWrapperType;
class DoubleWrapperTypeView;

// `DoubleWrapperType` is a special type which has no direct value
// representation. It is used to represent `google.protobuf.DoubleValue`, which
// never exists at runtime as a value. Its primary usage is for type checking
// and unpacking at runtime.
class DoubleWrapperType final {
 public:
  using view_alternative_type = DoubleWrapperTypeView;

  static constexpr TypeKind kKind = TypeKind::kDoubleWrapper;
  static constexpr absl::string_view kName = "google.protobuf.DoubleValue";

  explicit DoubleWrapperType(DoubleWrapperTypeView);

  DoubleWrapperType() = default;
  DoubleWrapperType(const DoubleWrapperType&) = default;
  DoubleWrapperType(DoubleWrapperType&&) = default;
  DoubleWrapperType& operator=(const DoubleWrapperType&) = default;
  DoubleWrapperType& operator=(DoubleWrapperType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kName;
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {};
  }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(DoubleWrapperType&) noexcept {}
};

inline constexpr void swap(DoubleWrapperType& lhs,
                           DoubleWrapperType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(DoubleWrapperType, DoubleWrapperType) {
  return true;
}

inline constexpr bool operator!=(DoubleWrapperType lhs, DoubleWrapperType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, DoubleWrapperType) {
  // DoubleWrapperType is really a singleton and all instances are equal.
  // Nothing to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out,
                                const DoubleWrapperType& type) {
  return out << type.DebugString();
}

class DoubleWrapperTypeView final {
 public:
  using alternative_type = DoubleWrapperType;

  static constexpr TypeKind kKind = DoubleWrapperType::kKind;
  static constexpr absl::string_view kName = DoubleWrapperType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  DoubleWrapperTypeView(
      const DoubleWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  DoubleWrapperTypeView& operator=(
      const DoubleWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  DoubleWrapperTypeView& operator=(DoubleWrapperType&&) = delete;

  DoubleWrapperTypeView() = default;
  DoubleWrapperTypeView(const DoubleWrapperTypeView&) = default;
  DoubleWrapperTypeView(DoubleWrapperTypeView&&) = default;
  DoubleWrapperTypeView& operator=(const DoubleWrapperTypeView&) = default;
  DoubleWrapperTypeView& operator=(DoubleWrapperTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(DoubleWrapperTypeView&) noexcept {}
};

inline constexpr void swap(DoubleWrapperTypeView& lhs,
                           DoubleWrapperTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(DoubleWrapperTypeView, DoubleWrapperTypeView) {
  return true;
}

inline constexpr bool operator!=(DoubleWrapperTypeView lhs,
                                 DoubleWrapperTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, DoubleWrapperTypeView) {
  // DoubleWrapperType is really a singleton and all instances are equal.
  // Nothing to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, DoubleWrapperTypeView type) {
  return out << type.DebugString();
}

inline DoubleWrapperType::DoubleWrapperType(DoubleWrapperTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_DOUBLE_WRAPPER_TYPE_H_
