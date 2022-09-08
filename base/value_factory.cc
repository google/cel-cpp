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

#include "base/value_factory.h"

#include <limits>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/value.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"

namespace cel {

namespace {

using base_internal::InlinedCordBytesValue;
using base_internal::InlinedCordStringValue;
using base_internal::InlinedStringViewBytesValue;
using base_internal::InlinedStringViewStringValue;
using base_internal::PersistentHandleFactory;
using base_internal::StringBytesValue;
using base_internal::StringStringValue;

}  // namespace

Persistent<const NullValue> NullValue::Get(ValueFactory& value_factory) {
  return value_factory.GetNullValue();
}

Persistent<const NullValue> ValueFactory::GetNullValue() {
  return PersistentHandleFactory<const NullValue>::Make<NullValue>();
}

Persistent<const ErrorValue> ValueFactory::CreateErrorValue(
    absl::Status status) {
  if (ABSL_PREDICT_FALSE(status.ok())) {
    status = absl::UnknownError(
        "If you are seeing this message the caller attempted to construct an "
        "error value from a successful status. Refusing to fail successfully.");
  }
  return PersistentHandleFactory<const ErrorValue>::Make<ErrorValue>(
      std::move(status));
}

Persistent<const BoolValue> BoolValue::False(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(false);
}

Persistent<const BoolValue> BoolValue::True(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(true);
}

Persistent<const DoubleValue> DoubleValue::NaN(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::quiet_NaN());
}

Persistent<const DoubleValue> DoubleValue::PositiveInfinity(
    ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::infinity());
}

Persistent<const DoubleValue> DoubleValue::NegativeInfinity(
    ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      -std::numeric_limits<double>::infinity());
}

Persistent<const DurationValue> DurationValue::Zero(
    ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateDurationValue(absl::ZeroDuration()).value();
}

Persistent<const TimestampValue> TimestampValue::UnixEpoch(
    ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateTimestampValue(absl::UnixEpoch()).value();
}

Persistent<const StringValue> StringValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetStringValue();
}

absl::StatusOr<Persistent<const StringValue>> StringValue::Concat(
    ValueFactory& value_factory, const StringValue& lhs,
    const StringValue& rhs) {
  absl::Cord cord;
  cord.Append(lhs.ToCord());
  cord.Append(rhs.ToCord());
  return value_factory.CreateStringValue(std::move(cord));
}

Persistent<const BytesValue> BytesValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetBytesValue();
}

absl::StatusOr<Persistent<const BytesValue>> BytesValue::Concat(
    ValueFactory& value_factory, const BytesValue& lhs, const BytesValue& rhs) {
  absl::Cord cord;
  cord.Append(lhs.ToCord());
  cord.Append(rhs.ToCord());
  return value_factory.CreateBytesValue(std::move(cord));
}

struct EnumType::NewInstanceVisitor final {
  const Persistent<const EnumType>& enum_type;
  ValueFactory& value_factory;

  absl::StatusOr<Persistent<const EnumValue>> operator()(
      absl::string_view name) const {
    TypedEnumValueFactory factory(value_factory, enum_type);
    return enum_type->NewInstanceByName(factory, name);
  }

  absl::StatusOr<Persistent<const EnumValue>> operator()(int64_t number) const {
    TypedEnumValueFactory factory(value_factory, enum_type);
    return enum_type->NewInstanceByNumber(factory, number);
  }
};

absl::StatusOr<Persistent<const EnumValue>> EnumValue::New(
    const Persistent<const EnumType>& enum_type, ValueFactory& value_factory,
    EnumType::ConstantId id) {
  CEL_ASSIGN_OR_RETURN(
      auto enum_value,
      absl::visit(EnumType::NewInstanceVisitor{enum_type, value_factory},
                  id.data_));
  if (!enum_value->type_) {
    // In case somebody is caching, we avoid setting the type_ if it has already
    // been set, to avoid a race condition where one CPU sees a half written
    // pointer.
    const_cast<EnumValue&>(*enum_value).type_ = enum_type;
  }
  return enum_value;
}

absl::StatusOr<Persistent<StructValue>> StructValue::New(
    const Persistent<const StructType>& struct_type,
    ValueFactory& value_factory) {
  TypedStructValueFactory factory(value_factory, struct_type);
  CEL_ASSIGN_OR_RETURN(auto struct_value, struct_type->NewInstance(factory));
  return struct_value;
}

Persistent<const BoolValue> ValueFactory::CreateBoolValue(bool value) {
  return PersistentHandleFactory<const BoolValue>::Make<BoolValue>(value);
}

Persistent<const IntValue> ValueFactory::CreateIntValue(int64_t value) {
  return PersistentHandleFactory<const IntValue>::Make<IntValue>(value);
}

Persistent<const UintValue> ValueFactory::CreateUintValue(uint64_t value) {
  return PersistentHandleFactory<const UintValue>::Make<UintValue>(value);
}

Persistent<const DoubleValue> ValueFactory::CreateDoubleValue(double value) {
  return PersistentHandleFactory<const DoubleValue>::Make<DoubleValue>(value);
}

absl::StatusOr<Persistent<const BytesValue>> ValueFactory::CreateBytesValue(
    std::string value) {
  if (value.empty()) {
    return GetEmptyBytesValue();
  }
  return PersistentHandleFactory<const BytesValue>::Make<StringBytesValue>(
      memory_manager(), std::move(value));
}

absl::StatusOr<Persistent<const BytesValue>> ValueFactory::CreateBytesValue(
    absl::Cord value) {
  if (value.empty()) {
    return GetEmptyBytesValue();
  }
  return PersistentHandleFactory<const BytesValue>::Make<InlinedCordBytesValue>(
      std::move(value));
}

absl::StatusOr<Persistent<const StringValue>> ValueFactory::CreateStringValue(
    std::string value) {
  // Avoid persisting empty strings which may have underlying storage after
  // mutating.
  if (value.empty()) {
    return GetEmptyStringValue();
  }
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return PersistentHandleFactory<const StringValue>::Make<StringStringValue>(
      memory_manager(), std::move(value));
}

absl::StatusOr<Persistent<const StringValue>> ValueFactory::CreateStringValue(
    absl::Cord value) {
  if (value.empty()) {
    return GetEmptyStringValue();
  }
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return PersistentHandleFactory<const StringValue>::Make<
      InlinedCordStringValue>(std::move(value));
}

absl::StatusOr<Persistent<const DurationValue>>
ValueFactory::CreateDurationValue(absl::Duration value) {
  CEL_RETURN_IF_ERROR(internal::ValidateDuration(value));
  return PersistentHandleFactory<const DurationValue>::Make<DurationValue>(
      value);
}

absl::StatusOr<Persistent<const TimestampValue>>
ValueFactory::CreateTimestampValue(absl::Time value) {
  CEL_RETURN_IF_ERROR(internal::ValidateTimestamp(value));
  return PersistentHandleFactory<const TimestampValue>::Make<TimestampValue>(
      value);
}

Persistent<const TypeValue> ValueFactory::CreateTypeValue(
    const Persistent<const Type>& value) {
  return PersistentHandleFactory<const TypeValue>::Make<TypeValue>(value);
}

Persistent<UnknownValue> ValueFactory::CreateUnknownValue(
    AttributeSet attribute_set, FunctionResultSet function_result_set) {
  return PersistentHandleFactory<UnknownValue>::Make<UnknownValue>(
      memory_manager(), std::move(attribute_set),
      std::move(function_result_set));
}

absl::StatusOr<Persistent<const BytesValue>>
ValueFactory::CreateBytesValueFromView(absl::string_view value) {
  return PersistentHandleFactory<const BytesValue>::Make<
      InlinedStringViewBytesValue>(value);
}

Persistent<const BytesValue> ValueFactory::GetEmptyBytesValue() {
  return PersistentHandleFactory<const BytesValue>::Make<
      InlinedStringViewBytesValue>(absl::string_view());
}

Persistent<const StringValue> ValueFactory::GetEmptyStringValue() {
  return PersistentHandleFactory<const StringValue>::Make<
      InlinedStringViewStringValue>(absl::string_view());
}

absl::StatusOr<Persistent<const StringValue>>
ValueFactory::CreateStringValueFromView(absl::string_view value) {
  return PersistentHandleFactory<const StringValue>::Make<
      InlinedStringViewStringValue>(value);
}

}  // namespace cel
