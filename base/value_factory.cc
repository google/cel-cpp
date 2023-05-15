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
#include "base/values/string_value.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"

namespace cel {

namespace {

using base_internal::HandleFactory;
using base_internal::InlinedCordBytesValue;
using base_internal::InlinedCordStringValue;
using base_internal::InlinedStringViewBytesValue;
using base_internal::InlinedStringViewStringValue;
using base_internal::StringBytesValue;
using base_internal::StringStringValue;

}  // namespace

Handle<NullValue> NullValue::Get(ValueFactory& value_factory) {
  return value_factory.GetNullValue();
}

Handle<ErrorValue> ValueFactory::CreateErrorValue(absl::Status status) {
  if (ABSL_PREDICT_FALSE(status.ok())) {
    status = absl::UnknownError(
        "If you are seeing this message the caller attempted to construct an "
        "error value from a successful status. Refusing to fail successfully.");
  }
  return HandleFactory<ErrorValue>::Make<ErrorValue>(std::move(status));
}

Handle<BoolValue> BoolValue::False(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(false);
}

Handle<BoolValue> BoolValue::True(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(true);
}

Handle<DoubleValue> DoubleValue::NaN(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::quiet_NaN());
}

Handle<DoubleValue> DoubleValue::PositiveInfinity(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::infinity());
}

Handle<DoubleValue> DoubleValue::NegativeInfinity(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      -std::numeric_limits<double>::infinity());
}

Handle<DurationValue> DurationValue::Zero(ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateDurationValue(absl::ZeroDuration()).value();
}

Handle<TimestampValue> TimestampValue::UnixEpoch(ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateTimestampValue(absl::UnixEpoch()).value();
}

Handle<StringValue> StringValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetStringValue();
}

absl::StatusOr<Handle<StringValue>> StringValue::Concat(
    ValueFactory& value_factory, const StringValue& lhs,
    const StringValue& rhs) {
  absl::Cord cord;
  cord.Append(lhs.ToCord());
  cord.Append(rhs.ToCord());
  return value_factory.CreateStringValue(std::move(cord));
}

Handle<BytesValue> BytesValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetBytesValue();
}

absl::StatusOr<Handle<BytesValue>> BytesValue::Concat(
    ValueFactory& value_factory, const BytesValue& lhs, const BytesValue& rhs) {
  absl::Cord cord;
  cord.Append(lhs.ToCord());
  cord.Append(rhs.ToCord());
  return value_factory.CreateBytesValue(std::move(cord));
}

absl::StatusOr<Handle<BytesValue>> ValueFactory::CreateBytesValue(
    std::string value) {
  if (value.empty()) {
    return GetEmptyBytesValue();
  }
  return HandleFactory<BytesValue>::Make<StringBytesValue>(memory_manager(),
                                                           std::move(value));
}

absl::StatusOr<Handle<BytesValue>> ValueFactory::CreateBytesValue(
    absl::Cord value) {
  if (value.empty()) {
    return GetEmptyBytesValue();
  }
  return HandleFactory<BytesValue>::Make<InlinedCordBytesValue>(
      std::move(value));
}

absl::StatusOr<Handle<StringValue>> ValueFactory::CreateStringValue(
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
  return HandleFactory<StringValue>::Make<StringStringValue>(memory_manager(),
                                                             std::move(value));
}

Handle<StringValue> ValueFactory::CreateUncheckedStringValue(
    std::string value) {
  // Avoid persisting empty strings which may have underlying storage after
  // mutating.
  if (value.empty()) {
    return GetEmptyStringValue();
  }

  return HandleFactory<StringValue>::Make<StringStringValue>(memory_manager(),
                                                             std::move(value));
}

Handle<StringValue> ValueFactory::CreateUncheckedStringValue(absl::Cord value) {
  // Avoid persisting empty strings which may have underlying storage after
  // mutating.
  if (value.empty()) {
    return GetEmptyStringValue();
  }

  return HandleFactory<StringValue>::Make<InlinedCordStringValue>(
      std::move(value));
}

absl::StatusOr<Handle<StringValue>> ValueFactory::CreateStringValue(
    absl::Cord value) {
  if (value.empty()) {
    return GetEmptyStringValue();
  }
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return HandleFactory<StringValue>::Make<InlinedCordStringValue>(
      std::move(value));
}

absl::StatusOr<Handle<DurationValue>> ValueFactory::CreateDurationValue(
    absl::Duration value) {
  CEL_RETURN_IF_ERROR(internal::ValidateDuration(value));
  return CreateUncheckedDurationValue(value);
}

absl::StatusOr<Handle<TimestampValue>> ValueFactory::CreateTimestampValue(
    absl::Time value) {
  CEL_RETURN_IF_ERROR(internal::ValidateTimestamp(value));
  return CreateUncheckedTimestampValue(value);
}

Handle<UnknownValue> ValueFactory::CreateUnknownValue(
    AttributeSet attribute_set, FunctionResultSet function_result_set) {
  return HandleFactory<UnknownValue>::Make<UnknownValue>(
      base_internal::UnknownSet(std::move(attribute_set),
                                std::move(function_result_set)));
}

absl::StatusOr<Handle<BytesValue>> ValueFactory::CreateBytesValueFromView(
    absl::string_view value) {
  return HandleFactory<BytesValue>::Make<InlinedStringViewBytesValue>(value);
}

absl::StatusOr<Handle<StringValue>> ValueFactory::CreateStringValueFromView(
    absl::string_view value) {
  return HandleFactory<StringValue>::Make<InlinedStringViewStringValue>(value);
}

}  // namespace cel
