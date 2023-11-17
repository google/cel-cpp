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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_ERROR_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_ERROR_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class ErrorType;
class ErrorTypeView;

// `ErrorType` is a special type which represents an error during type checking
// or an error value at runtime. See
// https://github.com/google/cel-spec/blob/master/doc/langdef.md#runtime-errors.
class ErrorType final {
 public:
  using view_alternative_type = ErrorTypeView;

  static constexpr TypeKind kKind = TypeKind::kError;
  static constexpr absl::string_view kName = "*error*";

  explicit ErrorType(ErrorTypeView);

  ErrorType() = default;
  ErrorType(const ErrorType&) = default;
  ErrorType(ErrorType&&) = default;
  ErrorType& operator=(const ErrorType&) = default;
  ErrorType& operator=(ErrorType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(ErrorType&) noexcept {}
};

inline constexpr void swap(ErrorType& lhs, ErrorType& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(const ErrorType&, const ErrorType&) {
  return true;
}

inline constexpr bool operator!=(const ErrorType& lhs, const ErrorType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const ErrorType& type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, const ErrorType& type) {
  return out << type.DebugString();
}

class ErrorTypeView final {
 public:
  using alternative_type = ErrorType;

  static constexpr TypeKind kKind = ErrorType::kKind;
  static constexpr absl::string_view kName = ErrorType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ErrorTypeView(const ErrorType& type ABSL_ATTRIBUTE_LIFETIME_BOUND
                    ABSL_ATTRIBUTE_UNUSED) noexcept {}

  ErrorTypeView() = default;
  ErrorTypeView(const ErrorTypeView&) = default;
  ErrorTypeView(ErrorTypeView&&) = default;
  ErrorTypeView& operator=(const ErrorTypeView&) = default;
  ErrorTypeView& operator=(ErrorTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

  constexpr void swap(ErrorTypeView&) noexcept {}
};

inline constexpr void swap(ErrorTypeView& lhs, ErrorTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline constexpr bool operator==(ErrorTypeView, ErrorTypeView) { return true; }

inline constexpr bool operator!=(ErrorTypeView lhs, ErrorTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, ErrorTypeView type) {
  return H::combine(std::move(state), type.kind());
}

inline std::ostream& operator<<(std::ostream& out, ErrorTypeView type) {
  return out << type.DebugString();
}

inline ErrorType::ErrorType(ErrorTypeView) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_ERROR_TYPE_H_
