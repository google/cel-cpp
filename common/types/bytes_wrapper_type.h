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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_BYTES_WRAPPER_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_BYTES_WRAPPER_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class BytesWrapperType;
class BytesWrapperTypeView;

// `BytesWrapperType` is a special type which has no direct value
// representation. It is used to represent `google.protobuf.BytesValue`, which
// never exists at runtime as a value. Its primary usage is for type checking
// and unpacking at runtime.
class BytesWrapperType final {
 public:
  using view_alternative_type = BytesWrapperTypeView;

  static constexpr TypeKind kKind = TypeKind::kBytesWrapper;
  static constexpr absl::string_view kName = "google.protobuf.BytesValue";

  explicit BytesWrapperType(BytesWrapperTypeView);

  BytesWrapperType() = default;
  BytesWrapperType(const BytesWrapperType&) = default;
  BytesWrapperType(BytesWrapperType&&) = default;
  BytesWrapperType& operator=(const BytesWrapperType&) = default;
  BytesWrapperType& operator=(BytesWrapperType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BytesWrapperType&) noexcept {}
};

inline constexpr void swap(BytesWrapperType& lhs,
                           BytesWrapperType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(BytesWrapperType, BytesWrapperType) {
  return true;
}

inline constexpr bool operator!=(BytesWrapperType lhs, BytesWrapperType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BytesWrapperType) {
  // BytesWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out,
                                const BytesWrapperType& type) {
  return out << type.DebugString();
}

class BytesWrapperTypeView final {
 public:
  using alternative_type = BytesWrapperType;

  static constexpr TypeKind kKind = BytesWrapperType::kKind;
  static constexpr absl::string_view kName = BytesWrapperType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  BytesWrapperTypeView(
      const BytesWrapperType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
          ABSL_ATTRIBUTE_UNUSED) noexcept {}

  BytesWrapperTypeView() = default;
  BytesWrapperTypeView(const BytesWrapperTypeView&) = default;
  BytesWrapperTypeView(BytesWrapperTypeView&&) = default;
  BytesWrapperTypeView& operator=(const BytesWrapperTypeView&) = default;
  BytesWrapperTypeView& operator=(BytesWrapperTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(BytesWrapperTypeView&) noexcept {}
};

inline constexpr void swap(BytesWrapperTypeView& lhs,
                           BytesWrapperTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(BytesWrapperTypeView, BytesWrapperTypeView) {
  return true;
}

inline constexpr bool operator!=(BytesWrapperTypeView lhs,
                                 BytesWrapperTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BytesWrapperTypeView) {
  // BytesWrapperType is really a singleton and all instances are equal. Nothing
  // to hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, BytesWrapperTypeView type) {
  return out << type.DebugString();
}

inline BytesWrapperType::BytesWrapperType(BytesWrapperTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_BYTES_WRAPPER_TYPE_H_
