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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRING_WRAPPER_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRING_WRAPPER_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class StringWrapperType;
class StringWrapperTypeView;

// `StringWrapperType` is a special type which has no direct value
// representation. It is used to represent `google.protobuf.StringValue`, which
// never exists at runtime as a value. Its primary usage is for type checking
// and unpacking at runtime.
class StringWrapperType final {
 public:
  using view_alternative_type = StringWrapperTypeView;

  static constexpr TypeKind kKind = TypeKind::kStringWrapper;
  static constexpr absl::string_view kName = "google.protobuf.StringValue";

  explicit StringWrapperType(StringWrapperTypeView);

  StringWrapperType() = default;
  StringWrapperType(const StringWrapperType&) = default;
  StringWrapperType(StringWrapperType&&) = default;
  StringWrapperType& operator=(const StringWrapperType&) = default;
  StringWrapperType& operator=(StringWrapperType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kName;
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {};
  }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(StringWrapperType&) noexcept {}
};

inline constexpr void swap(StringWrapperType& lhs,
                           StringWrapperType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(StringWrapperType, StringWrapperType) {
  return true;
}

inline constexpr bool operator!=(StringWrapperType lhs, StringWrapperType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, StringWrapperType) {
  // StringWrapperType is really a singleton and all instances are equal.
  // Nothing to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out,
                                const StringWrapperType& type) {
  return out << type.DebugString();
}

class StringWrapperTypeView final {
 public:
  using alternative_type = StringWrapperType;

  static constexpr TypeKind kKind = StringWrapperType::kKind;
  static constexpr absl::string_view kName = StringWrapperType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StringWrapperTypeView(
      const StringWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StringWrapperTypeView& operator=(
      const StringWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  StringWrapperTypeView& operator=(StringWrapperType&&) = delete;

  StringWrapperTypeView() = default;
  StringWrapperTypeView(const StringWrapperTypeView&) = default;
  StringWrapperTypeView(StringWrapperTypeView&&) = default;
  StringWrapperTypeView& operator=(const StringWrapperTypeView&) = default;
  StringWrapperTypeView& operator=(StringWrapperTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(StringWrapperTypeView&) noexcept {}
};

inline constexpr void swap(StringWrapperTypeView& lhs,
                           StringWrapperTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(StringWrapperTypeView, StringWrapperTypeView) {
  return true;
}

inline constexpr bool operator!=(StringWrapperTypeView lhs,
                                 StringWrapperTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, StringWrapperTypeView type) {
  // StringWrapperType is really a singleton and all instances are equal.
  // Nothing to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, StringWrapperTypeView type) {
  return out << type.DebugString();
}

inline StringWrapperType::StringWrapperType(StringWrapperTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRING_WRAPPER_TYPE_H_
