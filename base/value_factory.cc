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

using base_internal::ExternalDataBytesValue;
using base_internal::ExternalDataStringValue;
using base_internal::InlinedCordBytesValue;
using base_internal::InlinedCordStringValue;
using base_internal::InlinedStringViewBytesValue;
using base_internal::InlinedStringViewStringValue;
using base_internal::PersistentHandleFactory;
using base_internal::StringBytesValue;
using base_internal::StringStringValue;
using base_internal::TransientHandleFactory;

}  // namespace

Persistent<const NullValue> ValueFactory::GetNullValue() {
  return Persistent<const NullValue>(
      TransientHandleFactory<const NullValue>::MakeUnmanaged<const NullValue>(
          NullValue::Get()));
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
      memory_manager(), count, std::move(value));
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
  return CreateStringValue(std::move(value), count);
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

Persistent<const BytesValue> ValueFactory::GetEmptyBytesValue() {
  return PersistentHandleFactory<const BytesValue>::Make<
      InlinedStringViewBytesValue>(absl::string_view());
}

absl::StatusOr<Persistent<const BytesValue>> ValueFactory::CreateBytesValue(
    base_internal::ExternalData value) {
  return PersistentHandleFactory<const BytesValue>::Make<
      ExternalDataBytesValue>(memory_manager(), std::move(value));
}

Persistent<const StringValue> ValueFactory::GetEmptyStringValue() {
  return PersistentHandleFactory<const StringValue>::Make<
      InlinedStringViewStringValue>(absl::string_view());
}

absl::StatusOr<Persistent<const StringValue>> ValueFactory::CreateStringValue(
    absl::Cord value, size_t size) {
  if (value.empty()) {
    return GetEmptyStringValue();
  }
  return PersistentHandleFactory<const StringValue>::Make<
      InlinedCordStringValue>(size, std::move(value));
}

absl::StatusOr<Persistent<const StringValue>> ValueFactory::CreateStringValue(
    base_internal::ExternalData value) {
  return PersistentHandleFactory<const StringValue>::Make<
      ExternalDataStringValue>(memory_manager(), std::move(value));
}

}  // namespace cel
