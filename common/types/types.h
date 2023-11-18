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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_

#include <type_traits>

#include "absl/types/variant.h"

namespace cel {

class TypeInterface;

class Type;
class AnyType;
class BoolType;
class BoolWrapperType;
class BytesType;
class BytesWrapperType;
class DoubleType;
class DoubleWrapperType;
class DurationType;
class DynType;
class ErrorType;
class IntType;
class IntWrapperType;
class NullType;
class StringType;
class StringWrapperType;
class TimestampType;
class TypeType;
class UintType;
class UintWrapperType;
class UnknownType;

class TypeView;
class AnyTypeView;
class BoolTypeView;
class BoolWrapperTypeView;
class BytesTypeView;
class BytesWrapperTypeView;
class DoubleTypeView;
class DoubleWrapperTypeView;
class DurationTypeView;
class DynTypeView;
class ErrorTypeView;
class IntTypeView;
class IntWrapperTypeView;
class NullTypeView;
class StringTypeView;
class StringWrapperTypeView;
class TimestampTypeView;
class TypeTypeView;
class UintTypeView;
class UintWrapperTypeView;
class UnknownTypeView;

namespace common_internal {

template <typename T>
struct IsTypeAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<AnyType, T>, std::is_same<BoolType, T>,
          std::is_same<BoolWrapperType, T>, std::is_same<BytesType, T>,
          std::is_same<BytesWrapperType, T>, std::is_same<DoubleType, T>,
          std::is_same<DoubleWrapperType, T>, std::is_same<DurationType, T>,
          std::is_same<DynType, T>, std::is_same<ErrorType, T>,
          std::is_same<IntType, T>, std::is_same<IntWrapperType, T>,
          std::is_same<NullType, T>, std::is_same<StringType, T>,
          std::is_same<StringWrapperType, T>, std::is_same<TimestampType, T>,
          std::is_same<TypeType, T>, std::is_same<UintType, T>,
          std::is_same<UintWrapperType, T>, std::is_same<UnknownType, T>>> {};

template <typename T>
inline constexpr bool IsTypeAlternativeV = IsTypeAlternative<T>::value;

using TypeVariant = absl::variant<
// `absl::monostate` is used to detect use after moved-from, which invalidates
// `Type`. We only do this in debug builds to avoid paying the cost in
// production.
#ifndef NDEBUG
    absl::monostate,
#endif
    AnyType, BoolType, BoolWrapperType, BytesType, BytesWrapperType, DoubleType,
    DoubleWrapperType, DurationType, DynType, ErrorType, IntType,
    IntWrapperType, NullType, StringType, StringWrapperType, TimestampType,
    TypeType, UintType, UintWrapperType, UnknownType>;

template <typename T>
struct IsTypeViewAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<AnyTypeView, T>, std::is_same<BoolTypeView, T>,
          std::is_same<BoolWrapperTypeView, T>, std::is_same<BytesTypeView, T>,
          std::is_same<BytesWrapperTypeView, T>,
          std::is_same<DoubleTypeView, T>,
          std::is_same<DoubleWrapperTypeView, T>,
          std::is_same<DurationTypeView, T>, std::is_same<DynTypeView, T>,
          std::is_same<ErrorTypeView, T>, std::is_same<IntTypeView, T>,
          std::is_same<IntWrapperTypeView, T>, std::is_same<NullTypeView, T>,
          std::is_same<StringTypeView, T>,
          std::is_same<StringWrapperTypeView, T>,
          std::is_same<TimestampTypeView, T>, std::is_same<TypeTypeView, T>,
          std::is_same<UintTypeView, T>, std::is_same<UintWrapperTypeView, T>,
          std::is_same<UnknownTypeView, T>>> {};

template <typename T>
inline constexpr bool IsTypeViewAlternativeV = IsTypeViewAlternative<T>::value;

using TypeViewVariant = absl::variant<
// `absl::monostate` is used to detect use after moved-from, which invalidates
// `Type`. We only do this in debug builds to avoid paying the cost in
// production.
#ifndef NDEBUG
    absl::monostate,
#endif
    AnyTypeView, BoolTypeView, BoolWrapperTypeView, BytesTypeView,
    BytesWrapperTypeView, DoubleTypeView, DoubleWrapperTypeView,
    DurationTypeView, DynTypeView, ErrorTypeView, IntTypeView,
    IntWrapperTypeView, NullTypeView, StringTypeView, StringWrapperTypeView,
    TimestampTypeView, TypeTypeView, UintTypeView, UintWrapperTypeView,
    UnknownTypeView>;

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_
