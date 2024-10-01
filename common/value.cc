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

#include "common/value.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "common/json.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

static constexpr std::array<ValueKind, 25> kValueToKindArray = {
    ValueKind::kError,     ValueKind::kBool,     ValueKind::kBytes,
    ValueKind::kDouble,    ValueKind::kDuration, ValueKind::kError,
    ValueKind::kInt,       ValueKind::kList,     ValueKind::kList,
    ValueKind::kList,      ValueKind::kList,     ValueKind::kMap,
    ValueKind::kMap,       ValueKind::kMap,      ValueKind::kMap,
    ValueKind::kNull,      ValueKind::kOpaque,   ValueKind::kString,
    ValueKind::kStruct,    ValueKind::kStruct,   ValueKind::kStruct,
    ValueKind::kTimestamp, ValueKind::kType,     ValueKind::kUint,
    ValueKind::kUnknown};

static_assert(kValueToKindArray.size() ==
                  absl::variant_size<common_internal::ValueVariant>(),
              "Kind indexer must match variant declaration for cel::Value.");

}  // namespace

Type Value::GetRuntimeType() const {
  AssertIsValid();
  switch (kind()) {
    case ValueKind::kNull:
      return NullType();
    case ValueKind::kBool:
      return BoolType();
    case ValueKind::kInt:
      return IntType();
    case ValueKind::kUint:
      return UintType();
    case ValueKind::kDouble:
      return DoubleType();
    case ValueKind::kString:
      return StringType();
    case ValueKind::kBytes:
      return BytesType();
    case ValueKind::kStruct:
      return this->GetStruct().GetRuntimeType();
    case ValueKind::kDuration:
      return DurationType();
    case ValueKind::kTimestamp:
      return TimestampType();
    case ValueKind::kList:
      return ListType();
    case ValueKind::kMap:
      return MapType();
    case ValueKind::kUnknown:
      return UnknownType();
    case ValueKind::kType:
      return TypeType();
    case ValueKind::kError:
      return ErrorType();
    case ValueKind::kOpaque:
      return this->GetOpaque().GetRuntimeType();
    default:
      return cel::Type();
  }
}

ValueKind Value::kind() const {
  ABSL_DCHECK_NE(variant_.index(), 0)
      << "kind() called on uninitialized cel::Value.";
  return kValueToKindArray[variant_.index()];
}

absl::string_view Value::GetTypeName() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return absl::string_view();
        } else {
          return alternative.GetTypeName();
        }
      },
      variant_);
}

std::string Value::DebugString() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> std::string {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return std::string();
        } else {
          return alternative.DebugString();
        }
      },
      variant_);
}

absl::Status Value::SerializeTo(AnyToJsonConverter& value_manager,
                                absl::Cord& value) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &value](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.SerializeTo(value_manager, value);
        }
      },
      variant_);
}

absl::StatusOr<Json> Value::ConvertToJson(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<Json> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.ConvertToJson(value_manager);
        }
      },
      variant_);
}

absl::Status Value::Equal(ValueManager& value_manager, const Value& other,
                          Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.Equal(value_manager, other, result);
        }
      },
      variant_);
}

absl::StatusOr<Value> Value::Equal(ValueManager& value_manager,
                                   const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

bool Value::IsZeroValue() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> bool {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return false. In debug
          // builds we cannot reach here.
          return false;
        } else {
          return alternative.IsZeroValue();
        }
      },
      variant_);
}

std::ostream& operator<<(std::ostream& out, const Value& value) {
  return absl::visit(
      [&out](const auto& alternative) -> std::ostream& {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return out << "default ctor Value";
        } else {
          return out << alternative;
        }
      },
      value.variant_);
}

absl::StatusOr<Value> BytesValue::Equal(ValueManager& value_manager,
                                        const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ErrorValue::Equal(ValueManager& value_manager,
                                        const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ListValue::Equal(ValueManager& value_manager,
                                       const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> MapValue::Equal(ValueManager& value_manager,
                                      const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> OpaqueValue::Equal(ValueManager& value_manager,
                                         const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StringValue::Equal(ValueManager& value_manager,
                                         const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StructValue::Equal(ValueManager& value_manager,
                                         const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> TypeValue::Equal(ValueManager& value_manager,
                                       const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> UnknownValue::Equal(ValueManager& value_manager,
                                          const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::Status ListValue::Get(ValueManager& value_manager, size_t index,
                            Value& result) const {
  return absl::visit(
      [&value_manager, index,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Get(value_manager, index, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Get(ValueManager& value_manager,
                                     size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
}

absl::Status ListValue::ForEach(ValueManager& value_manager,
                                ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::Status ListValue::ForEach(ValueManager& value_manager,
                                ForEachWithIndexCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> ListValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::Status ListValue::Equal(ValueManager& value_manager, const Value& other,
                              Value& result) const {
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status ListValue::Contains(ValueManager& value_manager,
                                 const Value& other, Value& result) const {
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Contains(value_manager, other, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Contains(ValueManager& value_manager,
                                          const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

absl::Status MapValue::Get(ValueManager& value_manager, const Value& key,
                           Value& result) const {
  return absl::visit(
      [&value_manager, &key, &result](const auto& alternative) -> absl::Status {
        return alternative.Get(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Get(ValueManager& value_manager,
                                    const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

absl::StatusOr<bool> MapValue::Find(ValueManager& value_manager,
                                    const Value& key, Value& result) const {
  return absl::visit(
      [&value_manager, &key,
       &result](const auto& alternative) -> absl::StatusOr<bool> {
        return alternative.Find(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<std::pair<Value, bool>> MapValue::Find(
    ValueManager& value_manager, const Value& key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto ok, Find(value_manager, key, result));
  return std::pair{std::move(result), ok};
}

absl::Status MapValue::Has(ValueManager& value_manager, const Value& key,
                           Value& result) const {
  return absl::visit(
      [&value_manager, &key, &result](const auto& alternative) -> absl::Status {
        return alternative.Has(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Has(ValueManager& value_manager,
                                    const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

absl::Status MapValue::ListKeys(ValueManager& value_manager,
                                ListValue& result) const {
  return absl::visit(
      [&value_manager, &result](const auto& alternative) -> absl::Status {
        return alternative.ListKeys(value_manager, result);
      },
      variant_);
}

absl::StatusOr<ListValue> MapValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
}

absl::Status MapValue::ForEach(ValueManager& value_manager,
                               ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> MapValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::Status MapValue::Equal(ValueManager& value_manager, const Value& other,
                             Value& result) const {
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByName(value_manager, name, result,
                                            unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByName(value_manager, name, result, unboxing_options));
  return result;
}

absl::Status StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, result,
                                              unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByNumber(value_manager, number, result, unboxing_options));
  return result;
}

absl::Status StructValue::Equal(ValueManager& value_manager, const Value& other,
                                Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Equal(value_manager, other, result);
        }
      },
      variant_);
}

absl::Status StructValue::ForEachField(ValueManager& value_manager,
                                       ForEachFieldCallback callback) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ForEachField(value_manager, callback);
        }
      },
      variant_);
}

absl::StatusOr<int> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test,
       &result](const auto& alternative) -> absl::StatusOr<int> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     result);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<Value, int>> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(
      auto count, Qualify(value_manager, qualifiers, presence_test, result));
  return std::pair{std::move(result), count};
}

Value Value::Enum(absl::Nonnull<const google::protobuf::EnumValueDescriptor*> value) {
  ABSL_DCHECK(value != nullptr);
  if (value->type()->full_name() == "google.protobuf.NullValue") {
    ABSL_DCHECK_EQ(value->number(), 0);
    return NullValue();
  }
  return IntValue(value->number());
}

Value Value::Enum(absl::Nonnull<const google::protobuf::EnumDescriptor*> type,
                  int32_t number) {
  ABSL_DCHECK(type != nullptr);
  if (type->full_name() == "google.protobuf.NullValue") {
    ABSL_DCHECK_EQ(number, 0);
    return NullValue();
  }
  if (type->is_closed()) {
    if (ABSL_PREDICT_FALSE(type->FindValueByNumber(number) == nullptr)) {
      return ErrorValue(absl::InvalidArgumentError(absl::StrCat(
          "closed enum has no such value: ", type->full_name(), ".", number)));
    }
  }
  return IntValue(number);
}

absl::optional<BoolValue> Value::AsBool() const {
  if (const auto* alternative = absl::get_if<BoolValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const BytesValue> Value::AsBytes() const& {
  if (const auto* alternative = absl::get_if<BytesValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<BytesValue> Value::AsBytes() && {
  if (auto* alternative = absl::get_if<BytesValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<DoubleValue> Value::AsDouble() const {
  if (const auto* alternative = absl::get_if<DoubleValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<DurationValue> Value::AsDuration() const {
  if (const auto* alternative = absl::get_if<DurationValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const ErrorValue> Value::AsError() const& {
  if (const auto* alternative = absl::get_if<ErrorValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ErrorValue> Value::AsError() && {
  if (auto* alternative = absl::get_if<ErrorValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<IntValue> Value::AsInt() const {
  if (const auto* alternative = absl::get_if<IntValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ListValue> Value::AsList() const& {
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative =
          absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ListValue> Value::AsList() && {
  if (auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<MapValue> Value::AsMap() const& {
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MapValue> Value::AsMap() && {
  if (auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<MessageValue> Value::AsMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MessageValue> Value::AsMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<NullValue> Value::AsNull() const {
  if (const auto* alternative = absl::get_if<NullValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const OpaqueValue> Value::AsOpaque() const& {
  if (const auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<OpaqueValue> Value::AsOpaque() && {
  if (auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const OptionalValue> Value::AsOptional() const& {
  if (const auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr && alternative->IsOptional()) {
    return static_cast<const OptionalValue&>(*alternative);
  }
  return absl::nullopt;
}

absl::optional<OptionalValue> Value::AsOptional() && {
  if (auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr && alternative->IsOptional()) {
    return static_cast<OptionalValue&&>(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedJsonListValue> Value::AsParsedJsonList() const& {
  if (const auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedJsonListValue> Value::AsParsedJsonList() && {
  if (auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedJsonMapValue> Value::AsParsedJsonMap() const& {
  if (const auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedJsonMapValue> Value::AsParsedJsonMap() && {
  if (auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedListValue> Value::AsParsedList() const& {
  if (const auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedListValue> Value::AsParsedList() && {
  if (auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMapValue> Value::AsParsedMap() const& {
  if (const auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMapValue> Value::AsParsedMap() && {
  if (auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMapFieldValue> Value::AsParsedMapField() const& {
  if (const auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMapFieldValue> Value::AsParsedMapField() && {
  if (auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMessageValue> Value::AsParsedMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> Value::AsParsedMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedRepeatedFieldValue> Value::AsParsedRepeatedField()
    const& {
  if (const auto* alternative =
          absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedRepeatedFieldValue> Value::AsParsedRepeatedField() && {
  if (auto* alternative = absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedStructValue> Value::AsParsedStruct() const& {
  if (const auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedStructValue> Value::AsParsedStruct() && {
  if (auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const StringValue> Value::AsString() const& {
  if (const auto* alternative = absl::get_if<StringValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<StringValue> Value::AsString() && {
  if (auto* alternative = absl::get_if<StringValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<StructValue> Value::AsStruct() const& {
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<StructValue> Value::AsStruct() && {
  if (auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<TimestampValue> Value::AsTimestamp() const {
  if (const auto* alternative = absl::get_if<TimestampValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const TypeValue> Value::AsType() const& {
  if (const auto* alternative = absl::get_if<TypeValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<TypeValue> Value::AsType() && {
  if (auto* alternative = absl::get_if<TypeValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<UintValue> Value::AsUint() const {
  if (const auto* alternative = absl::get_if<UintValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const UnknownValue> Value::AsUnknown() const& {
  if (const auto* alternative = absl::get_if<UnknownValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<UnknownValue> Value::AsUnknown() && {
  if (auto* alternative = absl::get_if<UnknownValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

BoolValue Value::GetBool() const {
  ABSL_DCHECK(IsBool()) << *this;
  return absl::get<BoolValue>(variant_);
}

const BytesValue& Value::GetBytes() const& {
  ABSL_DCHECK(IsBytes()) << *this;
  return absl::get<BytesValue>(variant_);
}

BytesValue Value::GetBytes() && {
  ABSL_DCHECK(IsBytes()) << *this;
  return absl::get<BytesValue>(std::move(variant_));
}

DoubleValue Value::GetDouble() const {
  ABSL_DCHECK(IsDouble()) << *this;
  return absl::get<DoubleValue>(variant_);
}

DurationValue Value::GetDuration() const {
  ABSL_DCHECK(IsDuration()) << *this;
  return absl::get<DurationValue>(variant_);
}

const ErrorValue& Value::GetError() const& {
  ABSL_DCHECK(IsError()) << *this;
  return absl::get<ErrorValue>(variant_);
}

ErrorValue Value::GetError() && {
  ABSL_DCHECK(IsError()) << *this;
  return absl::get<ErrorValue>(std::move(variant_));
}

IntValue Value::GetInt() const {
  ABSL_DCHECK(IsInt()) << *this;
  return absl::get<IntValue>(variant_);
}

#ifdef ABSL_HAVE_EXCEPTIONS
#define CEL_VALUE_THROW_BAD_VARIANT_ACCESS() throw absl::bad_variant_access()
#else
#define CEL_VALUE_THROW_BAD_VARIANT_ACCESS() \
  ABSL_LOG(FATAL) << absl::bad_variant_access().what() /* Crash OK */
#endif

ListValue Value::GetList() const& {
  ABSL_DCHECK(IsList()) << *this;
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative =
          absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

ListValue Value::GetList() && {
  ABSL_DCHECK(IsList()) << *this;
  if (auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MapValue Value::GetMap() const& {
  ABSL_DCHECK(IsMap()) << *this;
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MapValue Value::GetMap() && {
  ABSL_DCHECK(IsMap()) << *this;
  if (auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MessageValue Value::GetMessage() const& {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

MessageValue Value::GetMessage() && {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

NullValue Value::GetNull() const {
  ABSL_DCHECK(IsNull()) << *this;
  return absl::get<NullValue>(variant_);
}

const OpaqueValue& Value::GetOpaque() const& {
  ABSL_DCHECK(IsOpaque()) << *this;
  return absl::get<OpaqueValue>(variant_);
}

OpaqueValue Value::GetOpaque() && {
  ABSL_DCHECK(IsOpaque()) << *this;
  return absl::get<OpaqueValue>(std::move(variant_));
}

const OptionalValue& Value::GetOptional() const& {
  ABSL_DCHECK(IsOptional()) << *this;
  return static_cast<const OptionalValue&>(absl::get<OpaqueValue>(variant_));
}

OptionalValue Value::GetOptional() && {
  ABSL_DCHECK(IsOptional()) << *this;
  return static_cast<OptionalValue&&>(
      absl::get<OpaqueValue>(std::move(variant_)));
}

const ParsedJsonListValue& Value::GetParsedJsonList() const& {
  ABSL_DCHECK(IsParsedJsonList()) << *this;
  return absl::get<ParsedJsonListValue>(variant_);
}

ParsedJsonListValue Value::GetParsedJsonList() && {
  ABSL_DCHECK(IsParsedJsonList()) << *this;
  return absl::get<ParsedJsonListValue>(std::move(variant_));
}

const ParsedJsonMapValue& Value::GetParsedJsonMap() const& {
  ABSL_DCHECK(IsParsedJsonMap()) << *this;
  return absl::get<ParsedJsonMapValue>(variant_);
}

ParsedJsonMapValue Value::GetParsedJsonMap() && {
  ABSL_DCHECK(IsParsedJsonMap()) << *this;
  return absl::get<ParsedJsonMapValue>(std::move(variant_));
}

const ParsedListValue& Value::GetParsedList() const& {
  ABSL_DCHECK(IsParsedList()) << *this;
  return absl::get<ParsedListValue>(variant_);
}

ParsedListValue Value::GetParsedList() && {
  ABSL_DCHECK(IsParsedList()) << *this;
  return absl::get<ParsedListValue>(std::move(variant_));
}

const ParsedMapValue& Value::GetParsedMap() const& {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedMapValue>(variant_);
}

ParsedMapValue Value::GetParsedMap() && {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedMapValue>(std::move(variant_));
}

const ParsedMapFieldValue& Value::GetParsedMapField() const& {
  ABSL_DCHECK(IsParsedMapField()) << *this;
  return absl::get<ParsedMapFieldValue>(variant_);
}

ParsedMapFieldValue Value::GetParsedMapField() && {
  ABSL_DCHECK(IsParsedMapField()) << *this;
  return absl::get<ParsedMapFieldValue>(std::move(variant_));
}

const ParsedMessageValue& Value::GetParsedMessage() const& {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

ParsedMessageValue Value::GetParsedMessage() && {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

const ParsedRepeatedFieldValue& Value::GetParsedRepeatedField() const& {
  ABSL_DCHECK(IsParsedRepeatedField()) << *this;
  return absl::get<ParsedRepeatedFieldValue>(variant_);
}

ParsedRepeatedFieldValue Value::GetParsedRepeatedField() && {
  ABSL_DCHECK(IsParsedRepeatedField()) << *this;
  return absl::get<ParsedRepeatedFieldValue>(std::move(variant_));
}

const ParsedStructValue& Value::GetParsedStruct() const& {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedStructValue>(variant_);
}

ParsedStructValue Value::GetParsedStruct() && {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedStructValue>(std::move(variant_));
}

const StringValue& Value::GetString() const& {
  ABSL_DCHECK(IsString()) << *this;
  return absl::get<StringValue>(variant_);
}

StringValue Value::GetString() && {
  ABSL_DCHECK(IsString()) << *this;
  return absl::get<StringValue>(std::move(variant_));
}

StructValue Value::GetStruct() const& {
  ABSL_DCHECK(IsStruct()) << *this;
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

StructValue Value::GetStruct() && {
  ABSL_DCHECK(IsStruct()) << *this;
  if (auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

TimestampValue Value::GetTimestamp() const {
  ABSL_DCHECK(IsTimestamp()) << *this;
  return absl::get<TimestampValue>(variant_);
}

const TypeValue& Value::GetType() const& {
  ABSL_DCHECK(IsType()) << *this;
  return absl::get<TypeValue>(variant_);
}

TypeValue Value::GetType() && {
  ABSL_DCHECK(IsType()) << *this;
  return absl::get<TypeValue>(std::move(variant_));
}

UintValue Value::GetUint() const {
  ABSL_DCHECK(IsUint()) << *this;
  return absl::get<UintValue>(variant_);
}

const UnknownValue& Value::GetUnknown() const& {
  ABSL_DCHECK(IsUnknown()) << *this;
  return absl::get<UnknownValue>(variant_);
}

UnknownValue Value::GetUnknown() && {
  ABSL_DCHECK(IsUnknown()) << *this;
  return absl::get<UnknownValue>(std::move(variant_));
}

namespace {

class EmptyValueIterator final : public ValueIterator {
 public:
  bool HasNext() override { return false; }

  absl::Status Next(ValueManager&, Value&) override {
    return absl::FailedPreconditionError(
        "`ValueIterator::Next` called after `ValueIterator::HasNext` returned "
        "false");
  }
};

}  // namespace

absl::Nonnull<std::unique_ptr<ValueIterator>> NewEmptyValueIterator() {
  return std::make_unique<EmptyValueIterator>();
}

}  // namespace cel
