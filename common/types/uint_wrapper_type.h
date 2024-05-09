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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_UINT_WRAPPER_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_UINT_WRAPPER_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class UintWrapperType;
class UintWrapperTypeView;

// `UintWrapperType` is a special type which has no direct value
// representation. It is used to represent `google.protobuf.UInt64Value`, which
// never exists at runtime as a value. Its primary usage is for type checking
// and unpacking at runtime.
class UintWrapperType final {
 public:
  using view_alternative_type = UintWrapperTypeView;

  static constexpr TypeKind kKind = TypeKind::kUintWrapper;
  static constexpr absl::string_view kName = "google.protobuf.UInt64Value";

  explicit UintWrapperType(UintWrapperTypeView);

  UintWrapperType() = default;
  UintWrapperType(const UintWrapperType&) = default;
  UintWrapperType(UintWrapperType&&) = default;
  UintWrapperType& operator=(const UintWrapperType&) = default;
  UintWrapperType& operator=(UintWrapperType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kName;
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {};
  }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(UintWrapperType&) noexcept {}
};

inline constexpr void swap(UintWrapperType& lhs,
                           UintWrapperType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(UintWrapperType, UintWrapperType) {
  return true;
}

inline constexpr bool operator!=(UintWrapperType lhs, UintWrapperType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, UintWrapperType) {
  // UintWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out,
                                const UintWrapperType& type) {
  return out << type.DebugString();
}

class UintWrapperTypeView final {
 public:
  using alternative_type = UintWrapperType;

  static constexpr TypeKind kKind = UintWrapperType::kKind;
  static constexpr absl::string_view kName = UintWrapperType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  UintWrapperTypeView(const UintWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                          ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  UintWrapperTypeView& operator=(
      const UintWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  UintWrapperTypeView& operator=(UintWrapperType&&) = delete;

  UintWrapperTypeView() = default;
  UintWrapperTypeView(const UintWrapperTypeView&) = default;
  UintWrapperTypeView(UintWrapperTypeView&&) = default;
  UintWrapperTypeView& operator=(const UintWrapperTypeView&) = default;
  UintWrapperTypeView& operator=(UintWrapperTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(UintWrapperTypeView&) noexcept {}
};

inline constexpr void swap(UintWrapperTypeView& lhs,
                           UintWrapperTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(UintWrapperTypeView, UintWrapperTypeView) {
  return true;
}

inline constexpr bool operator!=(UintWrapperTypeView lhs,
                                 UintWrapperTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, UintWrapperTypeView) {
  // UintWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, UintWrapperTypeView type) {
  return out << type.DebugString();
}

inline UintWrapperType::UintWrapperType(UintWrapperTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_UINT_WRAPPER_TYPE_H_
