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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRING_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRING_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class StringType;
class StringTypeView;

// `StringType` represents the primitive `string` type.
class StringType final {
 public:
  using view_alternative_type = StringTypeView;

  static constexpr TypeKind kKind = TypeKind::kString;
  static constexpr absl::string_view kName = "string";

  explicit StringType(StringTypeView);

  StringType() = default;
  StringType(const StringType&) = default;
  StringType(StringType&&) = default;
  StringType& operator=(const StringType&) = default;
  StringType& operator=(StringType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(StringType&) noexcept {}
};

inline constexpr void swap(StringType& lhs, StringType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(const StringType&, const StringType&) {
  return true;
}

inline constexpr bool operator!=(const StringType& lhs, const StringType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const StringType& type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, const StringType& type) {
  return out << type.DebugString();
}

class StringTypeView final {
 public:
  using alternative_type = StringType;

  static constexpr TypeKind kKind = StringType::kKind;
  static constexpr absl::string_view kName = StringType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StringTypeView(const StringType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                     ABSL_ATTRIBUTE_UNUSED) noexcept {}

  StringTypeView() = default;
  StringTypeView(const StringTypeView&) = default;
  StringTypeView(StringTypeView&&) = default;
  StringTypeView& operator=(const StringTypeView&) = default;
  StringTypeView& operator=(StringTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(StringTypeView&) noexcept {}
};

inline constexpr void swap(StringTypeView& lhs, StringTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(StringTypeView, StringTypeView) {
  return true;
}

inline constexpr bool operator!=(StringTypeView lhs, StringTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, StringTypeView type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, StringTypeView type) {
  return out << type.DebugString();
}

inline StringType::StringType(StringTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRING_TYPE_H_
