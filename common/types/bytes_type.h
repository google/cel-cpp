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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_BYTES_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_BYTES_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class BytesType;
class BytesTypeView;

// `BoolType` represents the primitive `bytes` type.
class BytesType final {
 public:
  using view_alternative_type = BytesTypeView;

  static constexpr TypeKind kKind = TypeKind::kBytes;
  static constexpr absl::string_view kName = "bytes";

  explicit BytesType(BytesTypeView);

  BytesType() = default;
  BytesType(const BytesType&) = default;
  BytesType(BytesType&&) = default;
  BytesType& operator=(const BytesType&) = default;
  BytesType& operator=(BytesType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kName;
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {};
  }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BytesType&) noexcept {}
};

inline constexpr void swap(BytesType& lhs, BytesType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(BytesType, BytesType) { return true; }

inline constexpr bool operator!=(BytesType lhs, BytesType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BytesType) {
  // BytesType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const BytesType& type) {
  return out << type.DebugString();
}

class BytesTypeView final {
 public:
  using alternative_type = BytesType;

  static constexpr TypeKind kKind = BytesType::kKind;
  static constexpr absl::string_view kName = BytesType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  BytesTypeView(const BytesType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                    ABSL_ATTRIBUTE_UNUSED) noexcept {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  BytesTypeView& operator=(const BytesType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                               ABSL_ATTRIBUTE_UNUSED) {
    return *this;
  }

  BytesTypeView& operator=(BytesType&&) = delete;

  BytesTypeView() = default;
  BytesTypeView(const BytesTypeView&) = default;
  BytesTypeView(BytesTypeView&&) = default;
  BytesTypeView& operator=(const BytesTypeView&) = default;
  BytesTypeView& operator=(BytesTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  absl::Span<const Type> parameters() const { return {}; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BytesTypeView&) noexcept {}
};

inline constexpr void swap(BytesTypeView& lhs, BytesTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(BytesTypeView, BytesTypeView) { return true; }

inline constexpr bool operator!=(BytesTypeView lhs, BytesTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BytesTypeView) {
  // BytesType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, BytesTypeView type) {
  return out << type.DebugString();
}

inline BytesType::BytesType(BytesTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_BYTES_TYPE_H_
