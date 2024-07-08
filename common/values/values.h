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

#include <memory>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/types/variant.h"

namespace cel {

class ValueManager;

class ValueInterface;
class ListValueInterface;
class MapValueInterface;
class StructValueInterface;

class Value;
class BoolValue;
class BytesValue;
class DoubleValue;
class DurationValue;
class ErrorValue;
class IntValue;
class ListValue;
class MapValue;
class NullValue;
class OpaqueValue;
class StringValue;
class StructValue;
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
class ListValueView;
class MapValueView;
class NullValueView;
class OpaqueValueView;
class StringValueView;
class StructValueView;
class TimestampValueView;
class TypeValueView;
class UintValueView;
class UnknownValueView;

class ParsedListValue;
class ParsedListValueView;
class ParsedListValueInterface;

class ParsedMapValue;
class ParsedMapValueView;
class ParsedMapValueInterface;

class ParsedStructValue;
class ParsedStructValueView;
class ParsedStructValueInterface;

class ValueIterator;
using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class OptionalValueView;

namespace common_internal {

class SharedByteString;
class SharedByteStringView;

class LegacyListValue;
class LegacyListValueView;

class LegacyMapValue;
class LegacyMapValueView;

class LegacyStructValue;
class LegacyStructValueView;

template <typename T>
struct IsListValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<ListValueInterface, T>>,
                             std::is_base_of<ListValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsListValueInterfaceV = IsListValueInterface<T>::value;

template <typename T>
struct IsListValueAlternative
    : std::bool_constant<std::disjunction_v<std::is_base_of<ParsedListValue, T>,
                                            std::is_same<LegacyListValue, T>>> {
};

template <typename T>
inline constexpr bool IsListValueAlternativeV =
    IsListValueAlternative<T>::value;

using ListValueVariant = absl::variant<ParsedListValue, LegacyListValue>;

template <typename T>
struct IsListValueViewAlternative
    : std::bool_constant<
          std::disjunction_v<std::is_base_of<ParsedListValueView, T>,
                             std::is_same<LegacyListValueView, T>>> {};

template <typename T>
inline constexpr bool IsListValueViewAlternativeV =
    IsListValueViewAlternative<T>::value;

using ListValueViewVariant =
    absl::variant<ParsedListValueView, LegacyListValueView>;

template <typename T>
struct IsMapValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<MapValueInterface, T>>,
                             std::is_base_of<MapValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsMapValueInterfaceV = IsMapValueInterface<T>::value;

template <typename T>
struct IsMapValueAlternative
    : std::bool_constant<std::disjunction_v<std::is_base_of<ParsedMapValue, T>,
                                            std::is_same<LegacyMapValue, T>>> {
};

template <typename T>
inline constexpr bool IsMapValueAlternativeV = IsMapValueAlternative<T>::value;

using MapValueVariant = absl::variant<ParsedMapValue, LegacyMapValue>;

template <typename T>
struct IsMapValueViewAlternative
    : std::bool_constant<
          std::disjunction_v<std::is_base_of<ParsedMapValueView, T>,
                             std::is_same<LegacyMapValueView, T>>> {};

template <typename T>
inline constexpr bool IsMapValueViewAlternativeV =
    IsMapValueViewAlternative<T>::value;

using MapValueViewVariant =
    absl::variant<ParsedMapValueView, LegacyMapValueView>;

template <typename T>
struct IsStructValueInterface
    : std::bool_constant<std::conjunction_v<
          std::negation<std::is_same<StructValueInterface, T>>,
          std::is_base_of<StructValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsStructValueInterfaceV =
    IsStructValueInterface<T>::value;

template <typename T>
struct IsStructValueAlternative
    : std::bool_constant<
          std::disjunction_v<std::is_base_of<ParsedStructValue, T>,
                             std::is_same<LegacyStructValue, T>>> {};

template <typename T>
inline constexpr bool IsStructValueAlternativeV =
    IsStructValueAlternative<T>::value;

using StructValueVariant =
    absl::variant<absl::monostate, ParsedStructValue, LegacyStructValue>;

template <typename T>
struct IsStructValueViewAlternative
    : std::bool_constant<
          std::disjunction_v<std::is_base_of<ParsedStructValueView, T>,
                             std::is_same<LegacyStructValueView, T>>> {};

template <typename T>
inline constexpr bool IsStructValueViewAlternativeV =
    IsStructValueViewAlternative<T>::value;

using StructValueViewVariant =
    absl::variant<absl::monostate, ParsedStructValueView,
                  LegacyStructValueView>;

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
          IsListValueAlternative<T>, IsMapValueAlternative<T>,
          std::is_same<NullValue, T>, std::is_base_of<OpaqueValue, T>,
          std::is_same<StringValue, T>, IsStructValueAlternative<T>,
          std::is_same<TimestampValue, T>, std::is_same<TypeValue, T>,
          std::is_same<UintValue, T>, std::is_same<UnknownValue, T>>> {};

template <typename T>
inline constexpr bool IsValueAlternativeV = IsValueAlternative<T>::value;

using ValueVariant = absl::variant<
    absl::monostate, BoolValue, BytesValue, DoubleValue, DurationValue,
    ErrorValue, IntValue, LegacyListValue, ParsedListValue, LegacyMapValue,
    ParsedMapValue, NullValue, OpaqueValue, StringValue, LegacyStructValue,
    ParsedStructValue, TimestampValue, TypeValue, UintValue, UnknownValue>;

template <typename T>
struct IsValueViewAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<BoolValueView, T>, std::is_same<BytesValueView, T>,
          std::is_same<DoubleValueView, T>, std::is_same<DurationValueView, T>,
          std::is_same<ErrorValueView, T>, std::is_same<IntValueView, T>,
          IsListValueViewAlternative<T>, IsMapValueViewAlternative<T>,
          std::is_same<NullValueView, T>, std::is_base_of<OpaqueValueView, T>,
          std::is_same<StringValueView, T>, IsStructValueViewAlternative<T>,
          std::is_same<TimestampValueView, T>, std::is_same<TypeValueView, T>,
          std::is_same<UintValueView, T>, std::is_same<UnknownValueView, T>>> {
};

template <typename T>
inline constexpr bool IsValueViewAlternativeV =
    IsValueViewAlternative<T>::value;

using ValueViewVariant =
    absl::variant<absl::monostate, BoolValueView, BytesValueView,
                  DoubleValueView, DurationValueView, ErrorValueView,
                  IntValueView, LegacyListValueView, ParsedListValueView,
                  LegacyMapValueView, ParsedMapValueView, NullValueView,
                  OpaqueValueView, StringValueView, LegacyStructValueView,
                  ParsedStructValueView, TimestampValueView, TypeValueView,
                  UintValueView, UnknownValueView>;

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
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedListValue, T>>> {
  using type = ParsedListValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<OpaqueValue, T>>> {
  using type = OpaqueValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedMapValue, T>>> {
  using type = ParsedMapValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedStructValue, T>>> {
  using type = ParsedStructValue;
};

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
struct BaseValueViewAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedListValueView, T>>> {
  using type = ParsedListValueView;
};

template <typename T>
struct BaseValueViewAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<OpaqueValueView, T>>> {
  using type = OpaqueValueView;
};

template <typename T>
struct BaseValueViewAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedMapValueView, T>>> {
  using type = ParsedMapValueView;
};

template <typename T>
struct BaseValueViewAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedStructValueView, T>>> {
  using type = ParsedStructValueView;
};

template <typename T>
using BaseValueViewAlternativeForT =
    typename BaseValueViewAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseListValueAlternativeFor {
  static_assert(IsListValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseListValueAlternativeFor<
    T, std::enable_if_t<IsListValueViewAlternativeV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseListValueAlternativeFor<T,
                                   std::enable_if_t<IsListValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseListValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedListValue, T>>> {
  using type = ParsedListValue;
};

template <typename T>
using BaseListValueAlternativeForT =
    typename BaseListValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseListValueViewAlternativeFor {
  static_assert(IsListValueViewAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseListValueViewAlternativeFor<
    T, std::enable_if_t<IsListValueAlternativeV<T>>>
    : BaseListValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
struct BaseListValueViewAlternativeFor<
    T, std::enable_if_t<IsListValueInterfaceV<T>>>
    : BaseListValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
struct BaseListValueViewAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedListValueView, T>>> {
  using type = ParsedListValueView;
};

template <typename T>
using BaseListValueViewAlternativeForT =
    typename BaseListValueViewAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseMapValueAlternativeFor {
  static_assert(IsMapValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseMapValueAlternativeFor<
    T, std::enable_if_t<IsMapValueViewAlternativeV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseMapValueAlternativeFor<T, std::enable_if_t<IsMapValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseMapValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedMapValue, T>>> {
  using type = ParsedMapValue;
};

template <typename T>
using BaseMapValueAlternativeForT =
    typename BaseMapValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseMapValueViewAlternativeFor {
  static_assert(IsMapValueViewAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseMapValueViewAlternativeFor<
    T, std::enable_if_t<IsMapValueAlternativeV<T>>>
    : BaseMapValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
struct BaseMapValueViewAlternativeFor<T,
                                      std::enable_if_t<IsMapValueInterfaceV<T>>>
    : BaseMapValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
struct BaseMapValueViewAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedMapValueView, T>>> {
  using type = ParsedMapValueView;
};

template <typename T>
using BaseMapValueViewAlternativeForT =
    typename BaseMapValueViewAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseStructValueAlternativeFor {
  static_assert(IsStructValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseStructValueAlternativeFor<
    T, std::enable_if_t<IsStructValueViewAlternativeV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseStructValueAlternativeFor<
    T, std::enable_if_t<IsStructValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseStructValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedStructValue, T>>> {
  using type = ParsedStructValue;
};

template <typename T>
using BaseStructValueAlternativeForT =
    typename BaseStructValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseStructValueViewAlternativeFor {
  static_assert(IsStructValueViewAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseStructValueViewAlternativeFor<
    T, std::enable_if_t<IsStructValueAlternativeV<T>>>
    : BaseStructValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
struct BaseStructValueViewAlternativeFor<
    T, std::enable_if_t<IsStructValueInterfaceV<T>>>
    : BaseStructValueViewAlternativeFor<typename T::view_alternative_type> {};

template <typename T>
struct BaseStructValueViewAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedStructValueView, T>>> {
  using type = ParsedStructValueView;
};

template <typename T>
using BaseStructValueViewAlternativeForT =
    typename BaseStructValueViewAlternativeFor<T>::type;

ErrorValueView GetDefaultErrorValue();

ParsedListValueView GetEmptyDynListValue();

ParsedMapValueView GetEmptyDynDynMapValue();

OptionalValueView GetEmptyDynOptionalValue();

absl::Status ListValueEqual(ValueManager& value_manager, const ListValue& lhs,
                            const ListValue& rhs, Value& result);

absl::Status ListValueEqual(ValueManager& value_manager,
                            const ParsedListValueInterface& lhs,
                            const ListValue& rhs, Value& result);

absl::Status MapValueEqual(ValueManager& value_manager, const MapValue& lhs,
                           const MapValue& rhs, Value& result);

absl::Status MapValueEqual(ValueManager& value_manager,
                           const ParsedMapValueInterface& lhs,
                           const MapValue& rhs, Value& result);

absl::Status StructValueEqual(ValueManager& value_manager,
                              const StructValue& lhs, const StructValue& rhs,
                              Value& result);

absl::Status StructValueEqual(ValueManager& value_manager,
                              const ParsedStructValueInterface& lhs,
                              const StructValue& rhs, Value& result);

const SharedByteString& AsSharedByteString(const BytesValue& value);
SharedByteStringView AsSharedByteStringView(BytesValueView value);

const SharedByteString& AsSharedByteString(const StringValue& value);
SharedByteStringView AsSharedByteStringView(StringValueView value);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
