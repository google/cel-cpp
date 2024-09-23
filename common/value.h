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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_interface.h"  // IWYU pragma: export
#include "common/value_kind.h"
#include "common/values/bool_value.h"  // IWYU pragma: export
#include "common/values/bytes_value.h"  // IWYU pragma: export
#include "common/values/double_value.h"  // IWYU pragma: export
#include "common/values/duration_value.h"  // IWYU pragma: export
#include "common/values/enum_value.h"  // IWYU pragma: export
#include "common/values/error_value.h"  // IWYU pragma: export
#include "common/values/int_value.h"  // IWYU pragma: export
#include "common/values/list_value.h"  // IWYU pragma: export
#include "common/values/map_value.h"  // IWYU pragma: export
#include "common/values/message_value.h"  // IWYU pragma: export
#include "common/values/null_value.h"  // IWYU pragma: export
#include "common/values/opaque_value.h"  // IWYU pragma: export
#include "common/values/optional_value.h"  // IWYU pragma: export
#include "common/values/parsed_json_list_value.h"  // IWYU pragma: export
#include "common/values/parsed_json_map_value.h"  // IWYU pragma: export
#include "common/values/parsed_map_field_value.h"  // IWYU pragma: export
#include "common/values/parsed_message_value.h"  // IWYU pragma: export
#include "common/values/parsed_repeated_field_value.h"  // IWYU pragma: export
#include "common/values/string_value.h"  // IWYU pragma: export
#include "common/values/struct_value.h"  // IWYU pragma: export
#include "common/values/timestamp_value.h"  // IWYU pragma: export
#include "common/values/type_value.h"  // IWYU pragma: export
#include "common/values/uint_value.h"  // IWYU pragma: export
#include "common/values/unknown_value.h"  // IWYU pragma: export
#include "common/values/values.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel {

class Value;

// `Value` is a composition type which encompasses all values supported by the
// Common Expression Language. When default constructed or moved, `Value` is in
// a known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
class Value final {
 public:
  // Returns an appropriate `Value` for the dynamic protobuf enum. For open
  // enums, returns `cel::IntValue`. For closed enums, returns `cel::ErrorValue`
  // if the value is not present in the enum otherwise returns `cel::IntValue`.
  static Value Enum(absl::Nonnull<const google::protobuf::EnumValueDescriptor*> value);
  static Value Enum(absl::Nonnull<const google::protobuf::EnumDescriptor*> type,
                    int32_t number);

  // SFINAE overload for generated protobuf enums which are not well-known.
  // Always returns `cel::IntValue`.
  template <typename T>
  static common_internal::EnableIfGeneratedEnum<T, IntValue> Enum(T value) {
    return IntValue(value);
  }

  // SFINAE overload for google::protobuf::NullValue. Always returns
  // `cel::NullValue`.
  template <typename T>
  static common_internal::EnableIfWellKnownEnum<T, google::protobuf::NullValue,
                                                NullValue>
  Enum(T) {
    return NullValue();
  }

  Value() = default;
  Value(const Value&) = default;
  Value& operator=(const Value&) = default;
  Value(Value&& other) = default;
  Value& operator=(Value&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ListValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ListValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ListValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ListValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedRepeatedFieldValue& value)
      : variant_(absl::in_place_type<ParsedRepeatedFieldValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedRepeatedFieldValue&& value)
      : variant_(absl::in_place_type<ParsedRepeatedFieldValue>,
                 std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedRepeatedFieldValue& value) {
    variant_.emplace<ParsedRepeatedFieldValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedRepeatedFieldValue&& value) {
    variant_.emplace<ParsedRepeatedFieldValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedJsonListValue& value)
      : variant_(absl::in_place_type<ParsedJsonListValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedJsonListValue&& value)
      : variant_(absl::in_place_type<ParsedJsonListValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedJsonListValue& value) {
    variant_.emplace<ParsedJsonListValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedJsonListValue&& value) {
    variant_.emplace<ParsedJsonListValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const MapValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(MapValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const MapValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(MapValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedMapFieldValue& value)
      : variant_(absl::in_place_type<ParsedMapFieldValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedMapFieldValue&& value)
      : variant_(absl::in_place_type<ParsedMapFieldValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedMapFieldValue& value) {
    variant_.emplace<ParsedMapFieldValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedMapFieldValue&& value) {
    variant_.emplace<ParsedMapFieldValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedJsonMapValue& value)
      : variant_(absl::in_place_type<ParsedJsonMapValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedJsonMapValue&& value)
      : variant_(absl::in_place_type<ParsedJsonMapValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedJsonMapValue& value) {
    variant_.emplace<ParsedJsonMapValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedJsonMapValue&& value) {
    variant_.emplace<ParsedJsonMapValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const StructValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(StructValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const StructValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(StructValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const MessageValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(MessageValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const MessageValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(MessageValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedMessageValue& value)
      : variant_(absl::in_place_type<ParsedMessageValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedMessageValue&& value)
      : variant_(absl::in_place_type<ParsedMessageValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedMessageValue& value) {
    variant_.emplace<ParsedMessageValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedMessageValue&& value) {
    variant_.emplace<ParsedMessageValue>(std::move(value));
    return *this;
  }

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const Shared<const T>& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            interface) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(Shared<const T>&& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            std::move(interface)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(T&& alternative) noexcept
      : variant_(absl::in_place_type<common_internal::BaseValueAlternativeForT<
                     absl::remove_cvref_t<T>>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(T&& type) noexcept {
    variant_.emplace<
        common_internal::BaseValueAlternativeForT<absl::remove_cvref_t<T>>>(
        std::forward<T>(type));
    return *this;
  }

  ValueKind kind() const;

  Type GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // `SerializeTo` serializes this value and appends it to `value`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::Status SerializeTo(AnyToJsonConverter& value_manager,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& value_manager) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  friend void swap(Value& lhs, Value& rhs) noexcept {
    using std::swap;
    swap(lhs.variant_, rhs.variant_);
  }

  friend std::ostream& operator<<(std::ostream& out, const Value& value);

  ABSL_DEPRECATED("Just use operator.()")
  Value* operator->() { return this; }

  ABSL_DEPRECATED("Just use operator.()")
  const Value* operator->() const { return this; }

  // Returns `true` if this value is an instance of a bool value.
  bool IsBool() const { return absl::holds_alternative<BoolValue>(variant_); }

  // Returns `true` if this value is an instance of a bytes value.
  bool IsBytes() const { return absl::holds_alternative<BytesValue>(variant_); }

  // Returns `true` if this value is an instance of a double value.
  bool IsDouble() const {
    return absl::holds_alternative<DoubleValue>(variant_);
  }

  // Returns `true` if this value is an instance of a duration value.
  bool IsDuration() const {
    return absl::holds_alternative<DurationValue>(variant_);
  }

  // Returns `true` if this value is an instance of an error value.
  bool IsError() const { return absl::holds_alternative<ErrorValue>(variant_); }

  // Returns `true` if this value is an instance of an int value.
  bool IsInt() const { return absl::holds_alternative<IntValue>(variant_); }

  // Returns `true` if this value is an instance of a list value.
  bool IsList() const {
    return absl::holds_alternative<common_internal::LegacyListValue>(
               variant_) ||
           absl::holds_alternative<ParsedListValue>(variant_) ||
           absl::holds_alternative<ParsedRepeatedFieldValue>(variant_) ||
           absl::holds_alternative<ParsedJsonListValue>(variant_);
  }

  // Returns `true` if this value is an instance of a map value.
  bool IsMap() const {
    return absl::holds_alternative<common_internal::LegacyMapValue>(variant_) ||
           absl::holds_alternative<ParsedMapValue>(variant_) ||
           absl::holds_alternative<ParsedMapFieldValue>(variant_) ||
           absl::holds_alternative<ParsedJsonMapValue>(variant_);
  }

  // Returns `true` if this value is an instance of a message value. If `true`
  // is returned, it is implied that `IsStruct()` would also return true.
  bool IsMessage() const {
    return absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  // Returns `true` if this value is an instance of a null value.
  bool IsNull() const { return absl::holds_alternative<NullValue>(variant_); }

  // Returns `true` if this value is an instance of an opaque value.
  bool IsOpaque() const {
    return absl::holds_alternative<OpaqueValue>(variant_);
  }

  // Returns `true` if this value is an instance of an optional value. If `true`
  // is returned, it is implied that `IsOpaque()` would also return true.
  bool IsOptional() const {
    if (const auto* alternative = absl::get_if<OpaqueValue>(&variant_);
        alternative != nullptr) {
      return alternative->IsOptional();
    }
    return false;
  }

  // Returns `true` if this value is an instance of a parsed JSON list value. If
  // `true` is returned, it is implied that `IsList()` would also return
  // true.
  bool IsParsedJsonList() const {
    return absl::holds_alternative<ParsedJsonListValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed JSON map value. If
  // `true` is returned, it is implied that `IsMap()` would also return
  // true.
  bool IsParsedJsonMap() const {
    return absl::holds_alternative<ParsedJsonMapValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed message value. If
  // `true` is returned, it is implied that `IsMessage()` would also return
  // true.
  bool IsParsedMessage() const {
    return absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  // Returns `true` if this value is an instance of a string value.
  bool IsString() const {
    return absl::holds_alternative<StringValue>(variant_);
  }

  // Returns `true` if this value is an instance of a struct value.
  bool IsStruct() const {
    return absl::holds_alternative<common_internal::LegacyStructValue>(
               variant_) ||
           absl::holds_alternative<ParsedStructValue>(variant_) ||
           absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  // Returns `true` if this value is an instance of a timestamp value.
  bool IsTimestamp() const {
    return absl::holds_alternative<TimestampValue>(variant_);
  }

  // Returns `true` if this value is an instance of a type value.
  bool IsType() const { return absl::holds_alternative<TypeValue>(variant_); }

  // Returns `true` if this value is an instance of a uint value.
  bool IsUint() const { return absl::holds_alternative<UintValue>(variant_); }

  // Returns `true` if this value is an instance of an unknown value.
  bool IsUnknown() const {
    return absl::holds_alternative<UnknownValue>(variant_);
  }

  // Convenience method for use with template metaprogramming. See
  // `IsBool()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, bool> Is() const {
    return IsBool();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsBytes()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, bool> Is() const {
    return IsBytes();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsDouble()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, bool> Is() const {
    return IsDouble();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsDuration()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, bool> Is() const {
    return IsDuration();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsError()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, bool> Is() const {
    return IsError();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsInt()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, bool> Is() const {
    return IsInt();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, bool> Is() const {
    return IsList();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, bool> Is() const {
    return IsMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, bool> Is() const {
    return IsMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsNull()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, bool> Is() const {
    return IsNull();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsOpaque()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, bool> Is() const {
    return IsOpaque();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsOptional()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, bool> Is() const {
    return IsOptional();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, bool> Is() const {
    return IsParsedMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsString()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, bool> Is() const {
    return IsString();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsStruct()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, bool> Is() const {
    return IsStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsTimestamp()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, bool> Is() const {
    return IsTimestamp();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsType()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, bool> Is() const {
    return IsType();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsUint()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, bool> Is() const {
    return IsUint();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsUnknown()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, bool> Is() const {
    return IsUnknown();
  }

  // Performs a checked cast from a value to a bool value,
  // returning a non-empty optional with either a value or reference to the
  // bool value. Otherwise an empty optional is returned.
  absl::optional<BoolValue> AsBool() const;

  // Performs a checked cast from a value to a bytes value,
  // returning a non-empty optional with either a value or reference to the
  // bytes value. Otherwise an empty optional is returned.
  optional_ref<const BytesValue> AsBytes() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const BytesValue> AsBytes() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<BytesValue> AsBytes() &&;
  absl::optional<BytesValue> AsBytes() const&&;

  // Performs a checked cast from a value to a double value,
  // returning a non-empty optional with either a value or reference to the
  // double value. Otherwise an empty optional is returned.
  absl::optional<DoubleValue> AsDouble() const;

  // Performs a checked cast from a value to a duration value,
  // returning a non-empty optional with either a value or reference to the
  // duration value. Otherwise an empty optional is returned.
  absl::optional<DurationValue> AsDuration() const;

  // Performs a checked cast from a value to an error value,
  // returning a non-empty optional with either a value or reference to the
  // error value. Otherwise an empty optional is returned.
  optional_ref<const ErrorValue> AsError() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const ErrorValue> AsError() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ErrorValue> AsError() &&;
  absl::optional<ErrorValue> AsError() const&&;

  // Performs a checked cast from a value to an int value,
  // returning a non-empty optional with either a value or reference to the
  // int value. Otherwise an empty optional is returned.
  absl::optional<IntValue> AsInt() const;

  // Performs a checked cast from a value to a list value,
  // returning a non-empty optional with either a value or reference to the
  // list value. Otherwise an empty optional is returned.
  absl::optional<ListValue> AsList() &;
  absl::optional<ListValue> AsList() const&;
  absl::optional<ListValue> AsList() &&;
  absl::optional<ListValue> AsList() const&&;

  // Performs a checked cast from a value to a map value,
  // returning a non-empty optional with either a value or reference to the
  // map value. Otherwise an empty optional is returned.
  absl::optional<MapValue> AsMap() &;
  absl::optional<MapValue> AsMap() const&;
  absl::optional<MapValue> AsMap() &&;
  absl::optional<MapValue> AsMap() const&&;

  // Performs a checked cast from a value to a message value,
  // returning a non-empty optional with either a value or reference to the
  // message value. Otherwise an empty optional is returned.
  absl::optional<MessageValue> AsMessage() &;
  absl::optional<MessageValue> AsMessage() const&;
  absl::optional<MessageValue> AsMessage() &&;
  absl::optional<MessageValue> AsMessage() const&&;

  // Performs a checked cast from a value to a null value,
  // returning a non-empty optional with either a value or reference to the
  // null value. Otherwise an empty optional is returned.
  absl::optional<NullValue> AsNull() const;

  // Performs a checked cast from a value to an opaque value,
  // returning a non-empty optional with either a value or reference to the
  // opaque value. Otherwise an empty optional is returned.
  optional_ref<const OpaqueValue> AsOpaque() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const OpaqueValue> AsOpaque()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<OpaqueValue> AsOpaque() &&;
  absl::optional<OpaqueValue> AsOpaque() const&&;

  // Performs a checked cast from a value to an optional value,
  // returning a non-empty optional with either a value or reference to the
  // optional value. Otherwise an empty optional is returned.
  optional_ref<const OptionalValue> AsOptional() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const OptionalValue> AsOptional()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<OptionalValue> AsOptional() &&;
  absl::optional<OptionalValue> AsOptional() const&&;

  // Performs a checked cast from a value to a parsed message value,
  // returning a non-empty optional with either a value or reference to the
  // parsed message value. Otherwise an empty optional is returned.
  optional_ref<const ParsedMessageValue> AsParsedMessage() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const ParsedMessageValue> AsParsedMessage()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedMessageValue> AsParsedMessage() &&;
  absl::optional<ParsedMessageValue> AsParsedMessage() const&&;

  // Performs a checked cast from a value to a string value,
  // returning a non-empty optional with either a value or reference to the
  // string value. Otherwise an empty optional is returned.
  optional_ref<const StringValue> AsString() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const StringValue> AsString()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<StringValue> AsString() &&;
  absl::optional<StringValue> AsString() const&&;

  // Performs a checked cast from a value to a struct value,
  // returning a non-empty optional with either a value or reference to the
  // struct value. Otherwise an empty optional is returned.
  absl::optional<StructValue> AsStruct() &;
  absl::optional<StructValue> AsStruct() const&;
  absl::optional<StructValue> AsStruct() &&;
  absl::optional<StructValue> AsStruct() const&&;

  // Performs a checked cast from a value to a timestamp value,
  // returning a non-empty optional with either a value or reference to the
  // timestamp value. Otherwise an empty optional is returned.
  absl::optional<TimestampValue> AsTimestamp() const;

  // Performs a checked cast from a value to a type value,
  // returning a non-empty optional with either a value or reference to the
  // type value. Otherwise an empty optional is returned.
  optional_ref<const TypeValue> AsType() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const TypeValue> AsType() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<TypeValue> AsType() &&;
  absl::optional<TypeValue> AsType() const&&;

  // Performs a checked cast from a value to an uint value,
  // returning a non-empty optional with either a value or reference to the
  // uint value. Otherwise an empty optional is returned.
  absl::optional<UintValue> AsUint() const;

  // Performs a checked cast from a value to an unknown value,
  // returning a non-empty optional with either a value or reference to the
  // unknown value. Otherwise an empty optional is returned.
  optional_ref<const UnknownValue> AsUnknown() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const UnknownValue> AsUnknown()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<UnknownValue> AsUnknown() &&;
  absl::optional<UnknownValue> AsUnknown() const&&;

  // Convenience method for use with template metaprogramming. See
  // `AsBool()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>>
  As() & {
    return AsBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>> As()
      const& {
    return AsBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>>
  As() && {
    return AsBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>> As()
      const&& {
    return AsBool();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsBytes()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<BytesValue, T>,
                       optional_ref<const BytesValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>,
                   optional_ref<const BytesValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, absl::optional<BytesValue>>
  As() && {
    return std::move(*this).AsBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, absl::optional<BytesValue>>
  As() const&& {
    return std::move(*this).AsBytes();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsDouble()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() & {
    return AsDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() const& {
    return AsDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() && {
    return AsDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() const&& {
    return AsDouble();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsDuration()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() & {
    return AsDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() const& {
    return AsDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() && {
    return AsDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() const&& {
    return AsDuration();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsError()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ErrorValue, T>,
                       optional_ref<const ErrorValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>,
                   optional_ref<const ErrorValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, absl::optional<ErrorValue>>
  As() && {
    return std::move(*this).AsError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, absl::optional<ErrorValue>>
  As() const&& {
    return std::move(*this).AsError();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsInt()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>>
  As() & {
    return AsInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>> As()
      const& {
    return AsInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>>
  As() && {
    return AsInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>> As()
      const&& {
    return AsInt();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>>
  As() & {
    return AsList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>> As()
      const& {
    return AsList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>>
  As() && {
    return std::move(*this).AsList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>> As()
      const&& {
    return std::move(*this).AsList();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>>
  As() & {
    return AsMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>> As()
      const& {
    return AsMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>>
  As() && {
    return std::move(*this).AsMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>> As()
      const&& {
    return std::move(*this).AsMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() & {
    return AsMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() const& {
    return AsMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() && {
    return std::move(*this).AsMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() const&& {
    return std::move(*this).AsMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsNull()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>>
  As() & {
    return AsNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>> As()
      const& {
    return AsNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>>
  As() && {
    return AsNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>> As()
      const&& {
    return AsNull();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsOpaque()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OpaqueValue, T>,
                       optional_ref<const OpaqueValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>,
                   optional_ref<const OpaqueValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, absl::optional<OpaqueValue>>
  As() && {
    return std::move(*this).AsOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, absl::optional<OpaqueValue>>
  As() const&& {
    return std::move(*this).AsOpaque();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsOptional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>,
                       optional_ref<const OptionalValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() && {
    return std::move(*this).AsOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() const&& {
    return std::move(*this).AsOptional();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedMessage()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       optional_ref<const ParsedMessageValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   optional_ref<const ParsedMessageValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() && {
    return std::move(*this).AsParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() const&& {
    return std::move(*this).AsParsedMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsString()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<StringValue, T>,
                       optional_ref<const StringValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>,
                   optional_ref<const StringValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, absl::optional<StringValue>>
  As() && {
    return std::move(*this).AsString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, absl::optional<StringValue>>
  As() const&& {
    return std::move(*this).AsString();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsStruct()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() & {
    return AsStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() const& {
    return AsStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() && {
    return std::move(*this).AsStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() const&& {
    return std::move(*this).AsStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsTimestamp()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() & {
    return AsTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() const& {
    return AsTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() && {
    return AsTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() const&& {
    return AsTimestamp();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsType()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<TypeValue, T>,
                       optional_ref<const TypeValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, optional_ref<const TypeValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, absl::optional<TypeValue>>
  As() && {
    return std::move(*this).AsType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, absl::optional<TypeValue>> As()
      const&& {
    return std::move(*this).AsType();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsUint()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>>
  As() & {
    return AsUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>> As()
      const& {
    return AsUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>>
  As() && {
    return AsUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>> As()
      const&& {
    return AsUint();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsUnknown()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<UnknownValue, T>,
                       optional_ref<const UnknownValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>,
                   optional_ref<const UnknownValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>,
                   absl::optional<UnknownValue>>
  As() && {
    return std::move(*this).AsUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>,
                   absl::optional<UnknownValue>>
  As() const&& {
    return std::move(*this).AsUnknown();
  }

  // Performs an unchecked cast from a value to a bool value. In
  // debug builds a best effort is made to crash. If `IsBool()` would return
  // false, calling this method is undefined behavior.
  BoolValue GetBool() const;

  // Performs an unchecked cast from a value to a bytes value. In
  // debug builds a best effort is made to crash. If `IsBytes()` would return
  // false, calling this method is undefined behavior.
  const BytesValue& GetBytes() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const BytesValue& GetBytes() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  BytesValue GetBytes() &&;
  BytesValue GetBytes() const&&;

  // Performs an unchecked cast from a value to a double value. In
  // debug builds a best effort is made to crash. If `IsDouble()` would return
  // false, calling this method is undefined behavior.
  DoubleValue GetDouble() const;

  // Performs an unchecked cast from a value to a duration value. In
  // debug builds a best effort is made to crash. If `IsDuration()` would return
  // false, calling this method is undefined behavior.
  DurationValue GetDuration() const;

  // Performs an unchecked cast from a value to an error value. In
  // debug builds a best effort is made to crash. If `IsError()` would return
  // false, calling this method is undefined behavior.
  const ErrorValue& GetError() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const ErrorValue& GetError() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ErrorValue GetError() &&;
  ErrorValue GetError() const&&;

  // Performs an unchecked cast from a value to an int value. In
  // debug builds a best effort is made to crash. If `IsInt()` would return
  // false, calling this method is undefined behavior.
  IntValue GetInt() const;

  // Performs an unchecked cast from a value to a list value. In
  // debug builds a best effort is made to crash. If `IsList()` would return
  // false, calling this method is undefined behavior.
  ListValue GetList() &;
  ListValue GetList() const&;
  ListValue GetList() &&;
  ListValue GetList() const&&;

  // Performs an unchecked cast from a value to a map value. In
  // debug builds a best effort is made to crash. If `IsMap()` would return
  // false, calling this method is undefined behavior.
  MapValue GetMap() &;
  MapValue GetMap() const&;
  MapValue GetMap() &&;
  MapValue GetMap() const&&;

  // Performs an unchecked cast from a value to a message value. In
  // debug builds a best effort is made to crash. If `IsMessage()` would return
  // false, calling this method is undefined behavior.
  MessageValue GetMessage() &;
  MessageValue GetMessage() const&;
  MessageValue GetMessage() &&;
  MessageValue GetMessage() const&&;

  // Performs an unchecked cast from a value to a null value. In
  // debug builds a best effort is made to crash. If `IsNull()` would return
  // false, calling this method is undefined behavior.
  NullValue GetNull() const;

  // Performs an unchecked cast from a value to an opaque value. In
  // debug builds a best effort is made to crash. If `IsOpaque()` would return
  // false, calling this method is undefined behavior.
  const OpaqueValue& GetOpaque() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const OpaqueValue& GetOpaque() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  OpaqueValue GetOpaque() &&;
  OpaqueValue GetOpaque() const&&;

  // Performs an unchecked cast from a value to an optional value. In
  // debug builds a best effort is made to crash. If `IsOptional()` would return
  // false, calling this method is undefined behavior.
  const OptionalValue& GetOptional() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const OptionalValue& GetOptional() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  OptionalValue GetOptional() &&;
  OptionalValue GetOptional() const&&;

  // Performs an unchecked cast from a value to a parsed message value. In
  // debug builds a best effort is made to crash. If `IsParsedJsonList()` would
  // return false, calling this method is undefined behavior.
  const ParsedJsonListValue& GetParsedJsonList() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const ParsedJsonListValue& GetParsedJsonList()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedJsonListValue GetParsedJsonList() &&;
  ParsedJsonListValue GetParsedJsonList() const&&;

  // Performs an unchecked cast from a value to a parsed message value. In
  // debug builds a best effort is made to crash. If `IsParsedJsonMap()` would
  // return false, calling this method is undefined behavior.
  const ParsedJsonMapValue& GetParsedJsonMap() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const ParsedJsonMapValue& GetParsedJsonMap()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedJsonMapValue GetParsedJsonMap() &&;
  ParsedJsonMapValue GetParsedJsonMap() const&&;

  // Performs an unchecked cast from a value to a parsed message value. In
  // debug builds a best effort is made to crash. If `IsParsedMessage()` would
  // return false, calling this method is undefined behavior.
  const ParsedMessageValue& GetParsedMessage() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const ParsedMessageValue& GetParsedMessage()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedMessageValue GetParsedMessage() &&;
  ParsedMessageValue GetParsedMessage() const&&;

  // Performs an unchecked cast from a value to a string value. In
  // debug builds a best effort is made to crash. If `IsString()` would return
  // false, calling this method is undefined behavior.
  const StringValue& GetString() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const StringValue& GetString() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  StringValue GetString() &&;
  StringValue GetString() const&&;

  // Performs an unchecked cast from a value to a struct value. In
  // debug builds a best effort is made to crash. If `IsStruct()` would return
  // false, calling this method is undefined behavior.
  StructValue GetStruct() &;
  StructValue GetStruct() const&;
  StructValue GetStruct() &&;
  StructValue GetStruct() const&&;

  // Performs an unchecked cast from a value to a timestamp value. In
  // debug builds a best effort is made to crash. If `IsTimestamp()` would
  // return false, calling this method is undefined behavior.
  TimestampValue GetTimestamp() const;

  // Performs an unchecked cast from a value to a type value. In
  // debug builds a best effort is made to crash. If `IsType()` would return
  // false, calling this method is undefined behavior.
  const TypeValue& GetType() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const TypeValue& GetType() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  TypeValue GetType() &&;
  TypeValue GetType() const&&;

  // Performs an unchecked cast from a value to an uint value. In
  // debug builds a best effort is made to crash. If `IsUint()` would return
  // false, calling this method is undefined behavior.
  UintValue GetUint() const;

  // Performs an unchecked cast from a value to an unknown value. In
  // debug builds a best effort is made to crash. If `IsUnknown()` would return
  // false, calling this method is undefined behavior.
  const UnknownValue& GetUnknown() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const UnknownValue& GetUnknown() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  UnknownValue GetUnknown() &&;
  UnknownValue GetUnknown() const&&;

  // Convenience method for use with template metaprogramming. See
  // `GetBool()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() & {
    return GetBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() const& {
    return GetBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() && {
    return GetBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() const&& {
    return GetBool();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetBytes()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<BytesValue, T>, const BytesValue&> Get() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, const BytesValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, BytesValue> Get() && {
    return std::move(*this).GetBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, BytesValue> Get() const&& {
    return std::move(*this).GetBytes();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetDouble()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() & {
    return GetDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() const& {
    return GetDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() && {
    return GetDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() const&& {
    return GetDouble();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetDuration()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get() & {
    return GetDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get()
      const& {
    return GetDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get() && {
    return GetDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get()
      const&& {
    return GetDuration();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetError()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ErrorValue, T>, const ErrorValue&> Get() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, const ErrorValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, ErrorValue> Get() && {
    return std::move(*this).GetError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, ErrorValue> Get() const&& {
    return std::move(*this).GetError();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetInt()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() & {
    return GetInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() const& {
    return GetInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() && {
    return GetInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() const&& {
    return GetInt();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() & {
    return GetList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() const& {
    return GetList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() && {
    return std::move(*this).GetList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() const&& {
    return std::move(*this).GetList();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() & {
    return GetMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() const& {
    return GetMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() && {
    return std::move(*this).GetMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() const&& {
    return std::move(*this).GetMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get() & {
    return GetMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get() const& {
    return GetMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get() && {
    return std::move(*this).GetMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get()
      const&& {
    return std::move(*this).GetMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetNull()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() & {
    return GetNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() const& {
    return GetNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() && {
    return GetNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() const&& {
    return GetNull();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetOpaque()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OpaqueValue, T>, const OpaqueValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, const OpaqueValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, OpaqueValue> Get() && {
    return std::move(*this).GetOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, OpaqueValue> Get() const&& {
    return std::move(*this).GetOpaque();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetOptional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get() && {
    return std::move(*this).GetOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get()
      const&& {
    return std::move(*this).GetOptional();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedJsonList()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                       const ParsedJsonListValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                   const ParsedJsonListValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>, ParsedJsonListValue>
  Get() && {
    return std::move(*this).GetParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>, ParsedJsonListValue>
  Get() const&& {
    return std::move(*this).GetParsedJsonList();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedJsonMap()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                       const ParsedJsonMapValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                   const ParsedJsonMapValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>, ParsedJsonMapValue>
  Get() && {
    return std::move(*this).GetParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>, ParsedJsonMapValue>
  Get() const&& {
    return std::move(*this).GetParsedJsonMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedMessage()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       const ParsedMessageValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   const ParsedMessageValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, ParsedMessageValue>
  Get() && {
    return std::move(*this).GetParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, ParsedMessageValue>
  Get() const&& {
    return std::move(*this).GetParsedMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetString()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<StringValue, T>, const StringValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, const StringValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, StringValue> Get() && {
    return std::move(*this).GetString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, StringValue> Get() const&& {
    return std::move(*this).GetString();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetStruct()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() & {
    return GetStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() const& {
    return GetStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() && {
    return std::move(*this).GetStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() const&& {
    return std::move(*this).GetStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetTimestamp()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get() & {
    return GetTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get()
      const& {
    return GetTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get() && {
    return GetTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get()
      const&& {
    return GetTimestamp();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetType()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<TypeValue, T>, const TypeValue&> Get() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, const TypeValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, TypeValue> Get() && {
    return std::move(*this).GetType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, TypeValue> Get() const&& {
    return std::move(*this).GetType();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetUint()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() & {
    return GetUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() const& {
    return GetUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() && {
    return GetUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() const&& {
    return GetUint();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetUnknown()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<UnknownValue, T>, const UnknownValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, const UnknownValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, UnknownValue> Get() && {
    return std::move(*this).GetUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, UnknownValue> Get()
      const&& {
    return std::move(*this).GetUnknown();
  }

  // When `Value` is default constructed, it is in a valid but undefined state.
  // Any attempt to use it invokes undefined behavior. This mention can be used
  // to test whether this value is valid.
  explicit operator bool() const { return IsValid(); }

 private:
  friend struct NativeTypeTraits<Value>;
  friend struct CompositionTraits<Value>;

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid Value";
  }

  common_internal::ValueVariant variant_;
};

template <>
struct NativeTypeTraits<Value> final {
  static NativeTypeId Id(const Value& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return
            // `NativeTypeId::For<absl::monostate>()`. In debug builds we cannot
            // reach here.
            return NativeTypeId::For<absl::monostate>();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        value.variant_);
  }

  static bool SkipDestructor(const Value& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just say we should skip the destructor.
            // In debug builds we cannot reach here.
            return true;
          } else {
            return NativeType::SkipDestructor(alternative);
          }
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<Value> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return absl::holds_alternative<Base>(value.variant_) &&
             InstanceOf<U>(Get<U>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyListValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedListValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyMapValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedMapValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyStructValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedStructValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, const U&>
  Get(const Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U&> Get(
      Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      Value&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedListValue>(std::move(value.variant_))};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedMapValue>(std::move(value.variant_))};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedStructValue>(std::move(value.variant_))};
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<Value, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<Value>);
static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_copy_assignable_v<Value>);
static_assert(std::is_nothrow_move_constructible_v<Value>);
static_assert(std::is_nothrow_move_assignable_v<Value>);
static_assert(std::is_nothrow_swappable_v<Value>);

using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class ValueIterator {
 public:
  virtual ~ValueIterator() = default;

  virtual bool HasNext() = 0;

  // Returns a view of the next value. If the underlying implementation cannot
  // directly return a view of a value, the value will be stored in `scratch`,
  // and the returned view will be that of `scratch`.
  virtual absl::Status Next(ValueManager& value_manager, Value& result) = 0;

  absl::StatusOr<Value> Next(ValueManager& value_manager) {
    Value result;
    CEL_RETURN_IF_ERROR(Next(value_manager, result));
    return result;
  }
};

class ValueBuilder {
 public:
  virtual ~ValueBuilder() = default;

  virtual absl::Status SetFieldByName(absl::string_view name, Value value) = 0;

  virtual absl::Status SetFieldByNumber(int64_t number, Value value) = 0;

  virtual Value Build() && = 0;
};

using ListValueBuilderInterface = ListValueBuilder;
using MapValueBuilderInterface = MapValueBuilder;
using StructValueBuilderInterface = StructValueBuilder;

// Now that Value is complete, we can define various parts of list, map, opaque,
// and struct which depend on Value.

inline absl::Status ParsedListValue::Get(ValueManager& value_manager,
                                         size_t index, Value& result) const {
  return interface_->Get(value_manager, index, result);
}

inline absl::Status ParsedListValue::ForEach(ValueManager& value_manager,
                                             ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::Status ParsedListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedListValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::Status ParsedListValue::Equal(ValueManager& value_manager,
                                           const Value& other,
                                           Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedListValue::Contains(ValueManager& value_manager,
                                              const Value& other,
                                              Value& result) const {
  return interface_->Contains(value_manager, other, result);
}

inline absl::Status OpaqueValue::Equal(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline cel::Value OptionalValueInterface::Value() const {
  cel::Value result;
  Value(result);
  return result;
}

inline void OptionalValue::Value(cel::Value& result) const {
  (*this)->Value(result);
}

inline cel::Value OptionalValue::Value() const { return (*this)->Value(); }

inline absl::Status ParsedMapValue::Get(ValueManager& value_manager,
                                        const Value& key, Value& result) const {
  return interface_->Get(value_manager, key, result);
}

inline absl::StatusOr<bool> ParsedMapValue::Find(ValueManager& value_manager,
                                                 const Value& key,
                                                 Value& result) const {
  return interface_->Find(value_manager, key, result);
}

inline absl::Status ParsedMapValue::Has(ValueManager& value_manager,
                                        const Value& key, Value& result) const {
  return interface_->Has(value_manager, key, result);
}

inline absl::Status ParsedMapValue::ListKeys(ValueManager& value_manager,
                                             ListValue& result) const {
  return interface_->ListKeys(value_manager, result);
}

inline absl::Status ParsedMapValue::ForEach(ValueManager& value_manager,
                                            ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedMapValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::Status ParsedMapValue::Equal(ValueManager& value_manager,
                                          const Value& other,
                                          Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return interface_->GetFieldByName(value_manager, name, result,
                                    unboxing_options);
}

inline absl::Status ParsedStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return interface_->GetFieldByNumber(value_manager, number, result,
                                      unboxing_options);
}

inline absl::Status ParsedStructValue::Equal(ValueManager& value_manager,
                                             const Value& other,
                                             Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedStructValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  return interface_->ForEachField(value_manager, callback);
}

inline absl::StatusOr<int> ParsedStructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  return interface_->Qualify(value_manager, qualifiers, presence_test, result);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
