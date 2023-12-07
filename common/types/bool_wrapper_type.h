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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_BOOL_WRAPPER_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_BOOL_WRAPPER_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class BoolWrapperType;
class BoolWrapperTypeView;

// `BoolWrapperType` is a special type which has no direct value representation.
// It is used to represent `google.protobuf.BoolValue`, which never exists at
// runtime as a value. Its primary usage is for type checking and unpacking at
// runtime.
class BoolWrapperType final {
 public:
  using view_alternative_type = BoolWrapperTypeView;

  static constexpr TypeKind kKind = TypeKind::kBoolWrapper;
  static constexpr absl::string_view kName = "google.protobuf.BoolValue";

  explicit BoolWrapperType(BoolWrapperTypeView);

  BoolWrapperType() = default;
  BoolWrapperType(const BoolWrapperType&) = default;
  BoolWrapperType(BoolWrapperType&&) = default;
  BoolWrapperType& operator=(const BoolWrapperType&) = default;
  BoolWrapperType& operator=(BoolWrapperType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BoolWrapperType&) noexcept {}
};

inline constexpr void swap(BoolWrapperType& lhs,
                           BoolWrapperType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(BoolWrapperType, BoolWrapperType) {
  return true;
}

inline constexpr bool operator!=(BoolWrapperType lhs, BoolWrapperType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BoolWrapperType) {
  // BoolWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out,
                                const BoolWrapperType& type) {
  return out << type.DebugString();
}

class BoolWrapperTypeView final {
 public:
  using alternative_type = BoolWrapperType;

  static constexpr TypeKind kKind = BoolWrapperType::kKind;
  static constexpr absl::string_view kName = BoolWrapperType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  BoolWrapperTypeView(const BoolWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                          ABSL_ATTRIBUTE_UNUSED) noexcept {}

  BoolWrapperTypeView() = default;
  BoolWrapperTypeView(const BoolWrapperTypeView&) = default;
  BoolWrapperTypeView(BoolWrapperTypeView&&) = default;
  BoolWrapperTypeView& operator=(const BoolWrapperTypeView&) = default;
  BoolWrapperTypeView& operator=(BoolWrapperTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BoolWrapperTypeView&) noexcept {}
};

inline constexpr void swap(BoolWrapperTypeView& lhs,
                           BoolWrapperTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(BoolWrapperTypeView, BoolWrapperTypeView) {
  return true;
}

inline constexpr bool operator!=(BoolWrapperTypeView lhs,
                                 BoolWrapperTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BoolWrapperTypeView) {
  // BoolWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, BoolWrapperTypeView type) {
  return out << type.DebugString();
}

inline BoolWrapperType::BoolWrapperType(BoolWrapperTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_BOOL_WRAPPER_TYPE_H_
