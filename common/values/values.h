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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {


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
class OptionalValue;
class StringValue;
class StructValue;
class TimestampValue;
class TypeValue;
class UintValue;
class UnknownValue;
class ParsedMessageValue;
class ParsedMapFieldValue;
class ParsedRepeatedFieldValue;
class ParsedJsonListValue;
class ParsedJsonMapValue;
class EnumValue;

class CustomListValue;
class CustomListValueInterface;

class CustomMapValue;
class CustomMapValueInterface;

class CustomStructValue;
class CustomStructValueInterface;

class ValueIterator;
using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class ValueIterator {
 public:
  virtual ~ValueIterator() = default;

  virtual bool HasNext() = 0;

  // Returns a view of the next value. If the underlying implementation cannot
  // directly return a view of a value, the value will be stored in `scratch`,
  // and the returned view will be that of `scratch`.
  virtual absl::Status Next(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) = 0;

  absl::StatusOr<Value> Next(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena);
};

namespace common_internal {

class SharedByteString;
class SharedByteStringView;

class LegacyListValue;

class LegacyMapValue;

class LegacyStructValue;

template <typename T>
struct IsListValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<ListValueInterface, T>>,
                             std::is_base_of<ListValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsListValueInterfaceV = IsListValueInterface<T>::value;

template <typename T>
struct IsListValueAlternative
    : std::bool_constant<std::disjunction_v<std::is_base_of<CustomListValue, T>,
                                            std::is_same<LegacyListValue, T>>> {
};

template <typename T>
inline constexpr bool IsListValueAlternativeV =
    IsListValueAlternative<T>::value;

using ListValueVariant =
    absl::variant<CustomListValue, LegacyListValue, ParsedRepeatedFieldValue,
                  ParsedJsonListValue>;

template <typename T>
struct IsMapValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<MapValueInterface, T>>,
                             std::is_base_of<MapValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsMapValueInterfaceV = IsMapValueInterface<T>::value;

template <typename T>
struct IsMapValueAlternative
    : std::bool_constant<std::disjunction_v<std::is_base_of<CustomMapValue, T>,
                                            std::is_same<LegacyMapValue, T>>> {
};

template <typename T>
inline constexpr bool IsMapValueAlternativeV = IsMapValueAlternative<T>::value;

using MapValueVariant = absl::variant<CustomMapValue, LegacyMapValue,
                                      ParsedMapFieldValue, ParsedJsonMapValue>;

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
          std::disjunction_v<std::is_base_of<CustomStructValue, T>,
                             std::is_same<LegacyStructValue, T>>> {};

template <typename T>
inline constexpr bool IsStructValueAlternativeV =
    IsStructValueAlternative<T>::value;

using StructValueVariant = absl::variant<absl::monostate, CustomStructValue,
                                         LegacyStructValue, ParsedMessageValue>;

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
          std::is_same<UintValue, T>, std::is_same<UnknownValue, T>,
          std::is_same<EnumValue, T>>> {};

template <typename T>
inline constexpr bool IsValueAlternativeV = IsValueAlternative<T>::value;

using ValueVariant =
    absl::variant<absl::monostate, BoolValue, BytesValue, DoubleValue,
                  DurationValue, ErrorValue, IntValue, LegacyListValue,
                  CustomListValue, ParsedRepeatedFieldValue,
                  ParsedJsonListValue, LegacyMapValue, CustomMapValue,
                  ParsedMapFieldValue, ParsedJsonMapValue, NullValue,
                  OpaqueValue, StringValue, LegacyStructValue,
                  CustomStructValue, ParsedMessageValue, TimestampValue,
                  TypeValue, UintValue, UnknownValue, EnumValue>;

// Get the base type alternative for the given alternative or interface. The
// base type alternative is the type stored in the `ValueVariant`.
template <typename T, typename = void>
struct BaseValueAlternativeFor {
  static_assert(IsValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseValueAlternativeFor<T, std::enable_if_t<IsValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<CustomListValue, T>>> {
  using type = CustomListValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<OpaqueValue, T>>> {
  using type = OpaqueValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<CustomMapValue, T>>> {
  using type = CustomMapValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<CustomStructValue, T>>> {
  using type = CustomStructValue;
};

template <typename T>
using BaseValueAlternativeForT = typename BaseValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseListValueAlternativeFor {
  static_assert(IsListValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseListValueAlternativeFor<T,
                                   std::enable_if_t<IsListValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseListValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<CustomListValue, T>>> {
  using type = CustomListValue;
};

template <typename T>
using BaseListValueAlternativeForT =
    typename BaseListValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseMapValueAlternativeFor {
  static_assert(IsMapValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseMapValueAlternativeFor<T, std::enable_if_t<IsMapValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseMapValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<CustomMapValue, T>>> {
  using type = CustomMapValue;
};

template <typename T>
using BaseMapValueAlternativeForT =
    typename BaseMapValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseStructValueAlternativeFor {
  static_assert(IsStructValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseStructValueAlternativeFor<
    T, std::enable_if_t<IsStructValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseStructValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<CustomStructValue, T>>> {
  using type = CustomStructValue;
};

template <typename T>
using BaseStructValueAlternativeForT =
    typename BaseStructValueAlternativeFor<T>::type;

ErrorValue GetDefaultErrorValue();

CustomListValue GetEmptyDynListValue();

CustomMapValue GetEmptyDynDynMapValue();

OptionalValue GetEmptyDynOptionalValue();

absl::Status ListValueEqual(
    const ListValue& lhs, const ListValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

absl::Status ListValueEqual(
    const CustomListValueInterface& lhs, const ListValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

absl::Status MapValueEqual(
    const MapValue& lhs, const MapValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

absl::Status MapValueEqual(
    const CustomMapValueInterface& lhs, const MapValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

absl::Status StructValueEqual(
    const StructValue& lhs, const StructValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

absl::Status StructValueEqual(
    const CustomStructValueInterface& lhs, const StructValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

const SharedByteString& AsSharedByteString(const BytesValue& value);

const SharedByteString& AsSharedByteString(const StringValue& value);

using ListValueForEachCallback =
    absl::FunctionRef<absl::StatusOr<bool>(const Value&)>;
using ListValueForEach2Callback =
    absl::FunctionRef<absl::StatusOr<bool>(size_t, const Value&)>;

template <typename Base>
class ValueMixin {
 public:
  absl::StatusOr<Value> Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  friend Base;
};

template <typename Base>
class ListValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  absl::StatusOr<Value> Get(
      size_t index,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  using ForEachCallback = absl::FunctionRef<absl::StatusOr<bool>(const Value&)>;

  absl::Status ForEach(
      ForEachCallback callback,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const {
    return static_cast<const Base*>(this)->ForEach(
        [callback](size_t, const Value& value) -> absl::StatusOr<bool> {
          return callback(value);
        },
        descriptor_pool, message_factory, arena);
  }

  absl::StatusOr<Value> Contains(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  friend Base;
};

template <typename Base>
class MapValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  absl::StatusOr<Value> Get(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::StatusOr<absl::optional<Value>> Find(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::StatusOr<Value> Has(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::StatusOr<ListValue> ListKeys(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  friend Base;
};

template <typename Base>
class StructValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  absl::StatusOr<Value> GetFieldByName(
      absl::string_view name,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::Status GetFieldByName(
      absl::string_view name,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
    return static_cast<const Base*>(this)->GetFieldByName(
        name, ProtoWrapperTypeOptions::kUnsetNull, descriptor_pool,
        message_factory, arena, result);
  }

  absl::StatusOr<Value> GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::StatusOr<Value> GetFieldByNumber(
      int64_t number,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::Status GetFieldByNumber(
      int64_t number,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
    return static_cast<const Base*>(this)->GetFieldByNumber(
        number, ProtoWrapperTypeOptions::kUnsetNull, descriptor_pool,
        message_factory, arena, result);
  }

  absl::StatusOr<Value> GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::StatusOr<std::pair<Value, int>> Qualify(
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  friend Base;
};

template <typename Base>
class OpaqueValueMixin : public ValueMixin<Base> {
 public:
  using ValueMixin<Base>::Equal;

  friend Base;
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
