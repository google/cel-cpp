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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_

#include <type_traits>

#include "absl/types/variant.h"

namespace cel {

class ValueInterface;

class Value;
class BoolValue;
class BytesValue;
class DoubleValue;
class DurationValue;
class ErrorValue;
class IntValue;
class NullValue;
class StringValue;
class TimestampValue;
class TypeValue;
class UintValue;
class UnknownValue;

class ValueView;
class BoolValueView;
class BytesValueView;
class DoubleValueView;
class DurationValueView;
class ErrorValueView;
class IntValueView;
class NullValueView;
class StringValueView;
class TimestampValueView;
class TypeValueView;
class UintValueView;
class UnknownValueView;

namespace common_internal {

template <typename T>
struct IsValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<ValueInterface, T>>,
                             std::is_base_of<ValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsValueInterfaceV = IsValueInterface<T>::value;

template <typename T>
struct IsValueAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<BoolValue, T>, std::is_same<BytesValue, T>,
          std::is_same<DoubleValue, T>, std::is_same<DurationValue, T>,
          std::is_same<ErrorValue, T>, std::is_same<IntValue, T>,
          std::is_same<NullValue, T>, std::is_same<StringValue, T>,
          std::is_same<TimestampValue, T>, std::is_same<TypeValue, T>,
          std::is_same<UintValue, T>, std::is_same<UnknownValue, T>>> {};

template <typename T>
inline constexpr bool IsValueAlternativeV = IsValueAlternative<T>::value;

using ValueVariant =
    absl::variant<absl::monostate, BoolValue, BytesValue, DoubleValue,
                  DurationValue, ErrorValue, IntValue, NullValue, StringValue,
                  TimestampValue, TypeValue, UintValue, UnknownValue>;

template <typename T>
struct IsValueViewAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<BoolValueView, T>, std::is_same<BytesValueView, T>,
          std::is_same<DoubleValueView, T>, std::is_same<DurationValueView, T>,
          std::is_same<ErrorValueView, T>, std::is_same<IntValueView, T>,
          std::is_same<NullValueView, T>, std::is_same<StringValueView, T>,
          std::is_same<TimestampValueView, T>, std::is_same<TypeValueView, T>,
          std::is_same<UintValueView, T>, std::is_same<UnknownValueView, T>>> {
};

template <typename T>
inline constexpr bool IsValueViewAlternativeV =
    IsValueViewAlternative<T>::value;

using ValueViewVariant =
    absl::variant<absl::monostate, BoolValueView, BytesValueView,
                  DoubleValueView, DurationValueView, ErrorValueView,
                  IntValueView, NullValueView, StringValueView,
                  TimestampValueView, TypeValueView, UintValueView,
                  UnknownValueView>;

// Get the base type alternative for the given alternative or interface. The
// base type alternative is the type stored in the `ValueVariant`.
template <typename T, typename = void>
struct BaseValueAlternativeFor {
  static_assert(IsValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseValueAlternativeFor<T, std::enable_if_t<IsValueViewAlternativeV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseValueAlternativeFor<T, std::enable_if_t<IsValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
using BaseValueAlternativeForT = typename BaseValueAlternativeFor<T>::type;

// Get the base type view alternative for the given alternative or interface.
// The base type view alternative is the type stored in the `ValueViewVariant`.
template <typename T, typename = void>
struct BaseValueViewAlternativeFor {
  static_assert(IsValueViewAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseValueViewAlternativeFor<T, std::enable_if_t<IsValueAlternativeV<T>>>
    : BaseValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
struct BaseValueViewAlternativeFor<T, std::enable_if_t<IsValueInterfaceV<T>>>
    : BaseValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
using BaseValueViewAlternativeForT =
    typename BaseValueViewAlternativeFor<T>::type;

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
