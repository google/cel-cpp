// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_KIND_H_
#define THIRD_PARTY_CEL_CPP_BASE_KIND_H_

#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/strings/string_view.h"

namespace cel {

enum class Kind /* : uint8_t */ {
  // Must match legacy CelValue::Type.
  kNull = 0,
  kBool,
  kInt,
  kUint,
  kDouble,
  kString,
  kBytes,
  kStruct,
  kDuration,
  kTimestamp,
  kList,
  kMap,
  kUnknown,
  kType,
  kError,
  kAny,

  // New kinds not present in legacy CelValue.
  kEnum,
  kDyn,
  kWrapper,
  kOpaque,

  // Legacy aliases, deprecated do not use.
  kNullType = kNull,
  kInt64 = kInt,
  kUint64 = kUint,
  kMessage = kStruct,
  kUnknownSet = kUnknown,
  kCelType = kType,

  // INTERNAL: Do not exceed 63. Implementation details rely on the fact that
  // we can store `Kind` using 6 bits.
  kNotForUseWithExhaustiveSwitchStatements = 63,
};

ABSL_ATTRIBUTE_PURE_FUNCTION absl::string_view KindToString(Kind kind);

// `TypeKind` is a subset of `Kind`, representing all valid `Kind` for `Type`.
// All `TypeKind` are valid `Kind`, but it is not guaranteed that all `Kind` are
// valid `TypeKind`.
enum class TypeKind : std::underlying_type_t<Kind> {
  kNull = static_cast<int>(Kind::kNull),
  kBool = static_cast<int>(Kind::kBool),
  kInt = static_cast<int>(Kind::kInt),
  kUint = static_cast<int>(Kind::kUint),
  kDouble = static_cast<int>(Kind::kDouble),
  kString = static_cast<int>(Kind::kString),
  kBytes = static_cast<int>(Kind::kBytes),
  kStruct = static_cast<int>(Kind::kStruct),
  kDuration = static_cast<int>(Kind::kDuration),
  kTimestamp = static_cast<int>(Kind::kTimestamp),
  kList = static_cast<int>(Kind::kList),
  kMap = static_cast<int>(Kind::kMap),
  kUnknown = static_cast<int>(Kind::kUnknown),
  kType = static_cast<int>(Kind::kType),
  kError = static_cast<int>(Kind::kError),
  kAny = static_cast<int>(Kind::kAny),
  kEnum = static_cast<int>(Kind::kEnum),
  kDyn = static_cast<int>(Kind::kDyn),
  kWrapper = static_cast<int>(Kind::kWrapper),
  kOpaque = static_cast<int>(Kind::kOpaque),

  // Legacy aliases, deprecated do not use.
  kNullType = kNull,
  kInt64 = kInt,
  kUint64 = kUint,
  kMessage = kStruct,
  kUnknownSet = kUnknown,
  kCelType = kType,

  // INTERNAL: Do not exceed 63. Implementation details rely on the fact that
  // we can store `Kind` using 6 bits.
  kNotForUseWithExhaustiveSwitchStatements =
      static_cast<int>(Kind::kNotForUseWithExhaustiveSwitchStatements),
};

constexpr Kind TypeKindToKind(TypeKind kind) {
  return static_cast<Kind>(static_cast<std::underlying_type_t<TypeKind>>(kind));
}

constexpr bool KindIsTypeKind(Kind kind ABSL_ATTRIBUTE_UNUSED) {
  // Currently all Kind are valid TypeKind.
  return true;
}

constexpr bool operator==(Kind lhs, TypeKind rhs) {
  return lhs == TypeKindToKind(rhs);
}

constexpr bool operator==(TypeKind lhs, Kind rhs) {
  return TypeKindToKind(lhs) == rhs;
}

constexpr bool operator!=(Kind lhs, TypeKind rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TypeKind lhs, Kind rhs) {
  return !operator==(lhs, rhs);
}

// `ValueKind` is a subset of `Kind`, representing all valid `Kind` for `Value`.
// All `ValueKind` are valid `Kind`, but it is not guaranteed that all `Kind`
// are valid `ValueKind`.
enum class ValueKind : std::underlying_type_t<Kind> {
  kNull = static_cast<int>(Kind::kNull),
  kBool = static_cast<int>(Kind::kBool),
  kInt = static_cast<int>(Kind::kInt),
  kUint = static_cast<int>(Kind::kUint),
  kDouble = static_cast<int>(Kind::kDouble),
  kString = static_cast<int>(Kind::kString),
  kBytes = static_cast<int>(Kind::kBytes),
  kStruct = static_cast<int>(Kind::kStruct),
  kDuration = static_cast<int>(Kind::kDuration),
  kTimestamp = static_cast<int>(Kind::kTimestamp),
  kList = static_cast<int>(Kind::kList),
  kMap = static_cast<int>(Kind::kMap),
  kUnknown = static_cast<int>(Kind::kUnknown),
  kType = static_cast<int>(Kind::kType),
  kError = static_cast<int>(Kind::kError),
  kEnum = static_cast<int>(Kind::kEnum),
  kOpaque = static_cast<int>(Kind::kOpaque),

  // Legacy aliases, deprecated do not use.
  kNullType = kNull,
  kInt64 = kInt,
  kUint64 = kUint,
  kMessage = kStruct,
  kUnknownSet = kUnknown,
  kCelType = kType,

  // INTERNAL: Do not exceed 63. Implementation details rely on the fact that
  // we can store `Kind` using 6 bits.
  kNotForUseWithExhaustiveSwitchStatements =
      static_cast<int>(Kind::kNotForUseWithExhaustiveSwitchStatements),
};

constexpr Kind ValueKindToKind(ValueKind kind) {
  return static_cast<Kind>(
      static_cast<std::underlying_type_t<ValueKind>>(kind));
}

constexpr bool KindIsValueKind(Kind kind) {
  return kind != Kind::kWrapper && kind != Kind::kDyn && kind != Kind::kAny;
}

constexpr bool operator==(Kind lhs, ValueKind rhs) {
  return lhs == ValueKindToKind(rhs);
}

constexpr bool operator==(ValueKind lhs, Kind rhs) {
  return ValueKindToKind(lhs) == rhs;
}

constexpr bool operator==(TypeKind lhs, ValueKind rhs) {
  return TypeKindToKind(lhs) == ValueKindToKind(rhs);
}

constexpr bool operator==(ValueKind lhs, TypeKind rhs) {
  return ValueKindToKind(lhs) == TypeKindToKind(rhs);
}

constexpr bool operator!=(Kind lhs, ValueKind rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(ValueKind lhs, Kind rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TypeKind lhs, ValueKind rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(ValueKind lhs, TypeKind rhs) {
  return !operator==(lhs, rhs);
}

inline absl::string_view TypeKindToString(TypeKind kind) {
  // All TypeKind are valid Kind.
  return KindToString(TypeKindToKind(kind));
}

inline absl::string_view ValueKindToString(ValueKind kind) {
  // All ValueKind are valid Kind.
  return KindToString(ValueKindToKind(kind));
}

constexpr TypeKind KindToTypeKind(Kind kind) {
  ABSL_ASSERT(KindIsTypeKind(kind));
  return static_cast<TypeKind>(static_cast<std::underlying_type_t<Kind>>(kind));
}

constexpr ValueKind KindToValueKind(Kind kind) {
  ABSL_ASSERT(KindIsValueKind(kind));
  return static_cast<ValueKind>(
      static_cast<std::underlying_type_t<Kind>>(kind));
}

constexpr ValueKind TypeKindToValueKind(TypeKind kind) {
  ABSL_ASSERT(KindIsValueKind(TypeKindToKind(kind)));
  return static_cast<ValueKind>(
      static_cast<std::underlying_type_t<TypeKind>>(kind));
}

constexpr TypeKind ValueKindToTypeKind(ValueKind kind) {
  ABSL_ASSERT(KindIsTypeKind(ValueKindToKind(kind)));
  return static_cast<TypeKind>(
      static_cast<std::underlying_type_t<ValueKind>>(kind));
}

static_assert(std::is_same_v<std::underlying_type_t<TypeKind>,
                             std::underlying_type_t<ValueKind>>,
              "TypeKind and ValueKind must have the same underlying type");

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_KIND_H_
