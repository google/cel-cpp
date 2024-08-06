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
class OpaqueTypeInterface;

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
class FunctionType;
class IntType;
class IntWrapperType;
class ListType;
class MapType;
class NullType;
class OpaqueType;
class OptionalType;
class StringType;
class StringWrapperType;
class StructType;
class MessageType;
class TimestampType;
class TypeParamType;
class TypeType;
class UintType;
class UintWrapperType;
class UnknownType;

namespace common_internal {

class BasicStructType;

template <typename Base, typename Derived>
struct IsDerivedFrom
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<Base, Derived>>,
                             std::is_base_of<Base, Derived>>> {};

template <typename Base, typename Derived>
inline constexpr bool IsDerivedFromV = IsDerivedFrom<Base, Derived>::value;

template <typename T>
struct IsTypeInterface : IsDerivedFrom<TypeInterface, T> {};

template <typename T>
inline constexpr bool IsTypeInterfaceV = IsTypeInterface<T>::value;

template <typename T>
struct IsTypeAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<AnyType, T>, std::is_same<BoolType, T>,
          std::is_same<BoolWrapperType, T>, std::is_same<BytesType, T>,
          std::is_same<BytesWrapperType, T>, std::is_same<DoubleType, T>,
          std::is_same<DoubleWrapperType, T>, std::is_same<DurationType, T>,
          std::is_same<DynType, T>, std::is_same<ErrorType, T>,
          std::is_same<FunctionType, T>, std::is_same<IntType, T>,
          std::is_same<IntWrapperType, T>, std::is_same<ListType, T>,
          std::is_same<MapType, T>, std::is_same<NullType, T>,
          std::is_base_of<OpaqueType, T>, std::is_same<StringType, T>,
          std::is_same<StringWrapperType, T>, std::is_base_of<MessageType, T>,
          std::is_base_of<BasicStructType, T>, std::is_same<TimestampType, T>,
          std::is_same<TypeParamType, T>, std::is_same<TypeType, T>,
          std::is_same<UintType, T>, std::is_same<UintWrapperType, T>,
          std::is_same<UnknownType, T>>> {};

template <typename T>
inline constexpr bool IsTypeAlternativeV = IsTypeAlternative<T>::value;

using TypeVariant =
    absl::variant<absl::monostate, AnyType, BoolType, BoolWrapperType,
                  BytesType, BytesWrapperType, DoubleType, DoubleWrapperType,
                  DurationType, DynType, ErrorType, FunctionType, IntType,
                  IntWrapperType, ListType, MapType, NullType, OpaqueType,
                  StringType, StringWrapperType, MessageType, BasicStructType,
                  TimestampType, TypeParamType, TypeType, UintType,
                  UintWrapperType, UnknownType>;

template <typename T>
struct IsStructTypeAlternative
    : std::bool_constant<std::disjunction_v<std::is_same<BasicStructType, T>,
                                            std::is_same<MessageType, T>>> {};

template <typename T>
inline constexpr bool IsStructTypeAlternativeV =
    IsStructTypeAlternative<T>::value;

using StructTypeVariant =
    absl::variant<absl::monostate, BasicStructType, MessageType>;

// Get the base type alternative for the given alternative or interface. The
// base type alternative is the type stored in the `TypeVariant`.
template <typename T, typename = void>
struct BaseTypeAlternativeFor {
  static_assert(IsTypeAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseTypeAlternativeFor<T, std::enable_if_t<IsTypeInterfaceV<T>>>
    : BaseTypeAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseTypeAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<OpaqueType, T>>> {
  using type = OpaqueType;
};

template <typename T>
using BaseTypeAlternativeForT = typename BaseTypeAlternativeFor<T>::type;

ListType GetDynListType();

MapType GetDynDynMapType();

OptionalType GetDynOptionalType();

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_
