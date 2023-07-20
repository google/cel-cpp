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

#include "extensions/protobuf/internal/wrappers.h"

#include <cmath>
#include <limits>
#include <string>

#include "google/protobuf/wrappers.pb.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "internal/casts.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions::protobuf_internal {

namespace {

template <typename P>
using FieldGetterRef =
    absl::FunctionRef<P(const google::protobuf::Reflection&, const google::protobuf::Message&,
                        const google::protobuf::FieldDescriptor*)>;

template <typename T, typename P>
using GeneratedUnwrapperRef = absl::FunctionRef<absl::StatusOr<P>(const T&)>;

template <typename P>
using FieldSetterRef =
    absl::FunctionRef<void(const google::protobuf::Reflection&, google::protobuf::Message*,
                           const google::protobuf::FieldDescriptor*, const P&)>;

template <typename T, typename P>
using GeneratedWrapperRef = absl::FunctionRef<absl::Status(const P&, T&)>;

template <typename T, typename P>
absl::StatusOr<P> UnwrapValueProto(const google::protobuf::Message& message,
                                   google::protobuf::FieldDescriptor::CppType cpp_type,
                                   GeneratedUnwrapperRef<T, P> unwrapper,
                                   FieldGetterRef<P> getter) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (ABSL_PREDICT_TRUE(desc == T::descriptor())) {
    // Fast path.
    return unwrapper(cel::internal::down_cast<const T&>(message));
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* value_field = desc->FindFieldByNumber(T::kValueFieldNumber);
  if (ABSL_PREDICT_FALSE(value_field == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing value field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(value_field->cpp_type() != cpp_type)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(),
        " has unexpected value field type: ", value_field->cpp_type_name()));
  }
  if (ABSL_PREDICT_FALSE(value_field->is_map() || value_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(),
                     " has unexpected value field cardinality: REPEATED"));
  }
  return getter(*reflect, message, value_field);
}

template <typename T, typename P>
absl::Status WrapValueProto(google::protobuf::Message& message, const P& value,
                            google::protobuf::FieldDescriptor::CppType cpp_type,
                            GeneratedWrapperRef<T, P> wrapper,
                            FieldSetterRef<P> setter) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (ABSL_PREDICT_TRUE(desc == T::descriptor())) {
    // Fast path.
    return wrapper(value, cel::internal::down_cast<T&>(message));
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* value_field = desc->FindFieldByNumber(T::kValueFieldNumber);
  if (ABSL_PREDICT_FALSE(value_field == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing value field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(value_field->cpp_type() != cpp_type)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(),
        " has unexpected value field type: ", value_field->cpp_type_name()));
  }
  if (ABSL_PREDICT_FALSE(value_field->is_map() || value_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(),
                     " has unexpected value field cardinality: REPEATED"));
  }
  setter(*reflect, &message, value_field, value);
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<bool> UnwrapDynamicBoolValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BoolValue");
  return UnwrapValueProto<google::protobuf::BoolValue, bool>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_BOOL,
      UnwrapGeneratedBoolValueProto, &google::protobuf::Reflection::GetBool);
}

absl::StatusOr<absl::Cord> UnwrapDynamicBytesValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BytesValue");
  return UnwrapValueProto<google::protobuf::BytesValue, absl::Cord>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      UnwrapGeneratedBytesValueProto, &google::protobuf::Reflection::GetCord);
}

absl::StatusOr<double> UnwrapDynamicFloatValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.FloatValue");
  return UnwrapValueProto<google::protobuf::FloatValue, double>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_FLOAT,
      UnwrapGeneratedFloatValueProto, &google::protobuf::Reflection::GetFloat);
}

absl::StatusOr<double> UnwrapDynamicDoubleValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.DoubleValue");
  return UnwrapValueProto<google::protobuf::DoubleValue, double>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE,
      UnwrapGeneratedDoubleValueProto, &google::protobuf::Reflection::GetDouble);
}

absl::StatusOr<int64_t> UnwrapDynamicInt32ValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int32Value");
  return UnwrapValueProto<google::protobuf::Int32Value, int64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_INT32,
      UnwrapGeneratedInt32ValueProto, &google::protobuf::Reflection::GetInt32);
}

absl::StatusOr<int64_t> UnwrapDynamicInt64ValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int64Value");
  return UnwrapValueProto<google::protobuf::Int64Value, int64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_INT64,
      UnwrapGeneratedInt64ValueProto, &google::protobuf::Reflection::GetInt64);
}

absl::StatusOr<absl::Cord> UnwrapDynamicStringValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.StringValue");
  return UnwrapValueProto<google::protobuf::StringValue, absl::Cord>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      UnwrapGeneratedStringValueProto, &google::protobuf::Reflection::GetCord);
}

absl::StatusOr<uint64_t> UnwrapDynamicUInt32ValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt32Value");
  return UnwrapValueProto<google::protobuf::UInt32Value, uint64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_UINT32,
      UnwrapGeneratedUInt32ValueProto, &google::protobuf::Reflection::GetUInt32);
}

absl::StatusOr<uint64_t> UnwrapDynamicUInt64ValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt64Value");
  return UnwrapValueProto<google::protobuf::UInt64Value, uint64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_UINT64,
      UnwrapGeneratedUInt64ValueProto, &google::protobuf::Reflection::GetUInt64);
}

absl::StatusOr<int64_t> UnwrapDynamicSignedIntegralValueProto(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  const auto& full_name = desc->full_name();
  if (full_name == "google.protobuf.Int32Value") {
    return UnwrapDynamicInt32ValueProto(message);
  }
  if (full_name == "google.protobuf.Int64Value") {
    return UnwrapDynamicInt64ValueProto(message);
  }
  auto status = absl::StrCat(full_name, " is not int-like");
  ABSL_DLOG(FATAL) << status;
  return absl::InvalidArgumentError(status);
}

absl::StatusOr<uint64_t> UnwrapDynamicUnsignedIntegralValueProto(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  const auto& full_name = desc->full_name();
  if (full_name == "google.protobuf.UInt32Value") {
    return UnwrapDynamicUInt32ValueProto(message);
  }
  if (full_name == "google.protobuf.UInt64Value") {
    return UnwrapDynamicUInt64ValueProto(message);
  }
  auto status = absl::StrCat(full_name, " is not uint-like");
  ABSL_DLOG(FATAL) << status;
  return absl::InvalidArgumentError(status);
}

absl::StatusOr<double> UnwrapDynamicFloatingPointValueProto(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  const auto& full_name = desc->full_name();
  if (full_name == "google.protobuf.FloatValue") {
    return UnwrapDynamicFloatValueProto(message);
  }
  if (full_name == "google.protobuf.DoubleValue") {
    return UnwrapDynamicDoubleValueProto(message);
  }
  auto status = absl::StrCat(full_name, " is not double-like");
  ABSL_DLOG(FATAL) << status;
  return absl::InvalidArgumentError(status);
}

absl::Status WrapDynamicBoolValueProto(bool value, google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BoolValue");
  return WrapValueProto<google::protobuf::BoolValue, bool>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_BOOL,
      WrapGeneratedBoolValueProto, &google::protobuf::Reflection::SetBool);
}

absl::Status WrapDynamicBytesValueProto(const absl::Cord& value,
                                        google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BytesValue");
  return WrapValueProto<google::protobuf::BytesValue, absl::Cord>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      WrapGeneratedBytesValueProto,
      [](const google::protobuf::Reflection& reflection, google::protobuf::Message* message,
         const google::protobuf::FieldDescriptor* field,
         const absl::Cord& value) -> void {
        reflection.SetString(message, field, value);
      });
}

absl::Status WrapDynamicFloatValueProto(float value, google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.FloatValue");
  return WrapValueProto<google::protobuf::FloatValue, float>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_FLOAT,
      WrapGeneratedFloatValueProto, &google::protobuf::Reflection::SetFloat);
}

absl::Status WrapDynamicDoubleValueProto(double value,
                                         google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.DoubleValue");
  return WrapValueProto<google::protobuf::DoubleValue, double>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE,
      WrapGeneratedDoubleValueProto, &google::protobuf::Reflection::SetDouble);
}

absl::Status WrapDynamicInt32ValueProto(int32_t value,
                                        google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int32Value");
  return WrapValueProto<google::protobuf::Int32Value, int32_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_INT32,
      WrapGeneratedInt32ValueProto, &google::protobuf::Reflection::SetInt32);
}

absl::Status WrapDynamicInt64ValueProto(int64_t value,
                                        google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int64Value");
  return WrapValueProto<google::protobuf::Int64Value, int64_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_INT64,
      WrapGeneratedInt64ValueProto, &google::protobuf::Reflection::SetInt64);
}

absl::Status WrapDynamicUInt32ValueProto(uint32_t value,
                                         google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt32Value");
  return WrapValueProto<google::protobuf::UInt32Value, uint32_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_UINT32,
      WrapGeneratedUInt32ValueProto, &google::protobuf::Reflection::SetUInt32);
}

absl::Status WrapDynamicUInt64ValueProto(uint64_t value,
                                         google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt64Value");
  return WrapValueProto<google::protobuf::UInt64Value, uint64_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_UINT64,
      WrapGeneratedUInt64ValueProto, &google::protobuf::Reflection::SetUInt64);
}

absl::Status WrapDynamicStringValueProto(const absl::Cord& value,
                                         google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.StringValue");
  return WrapValueProto<google::protobuf::StringValue, absl::Cord>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      WrapGeneratedStringValueProto,
      [](const google::protobuf::Reflection& reflection, google::protobuf::Message* message,
         const google::protobuf::FieldDescriptor* field,
         const absl::Cord& value) -> void {
        reflection.SetString(message, field, static_cast<std::string>(value));
      });
}

absl::Status WrapDynamicSignedIntegralValueProto(int64_t value,
                                                 google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  const auto& full_name = desc->full_name();
  if (full_name == "google.protobuf.Int32Value") {
    if (ABSL_PREDICT_FALSE(value < std::numeric_limits<int32_t>::min() ||
                           value > std::numeric_limits<int32_t>::max())) {
      return absl::OutOfRangeError("int64 out of int32_t range");
    }
    return WrapDynamicInt32ValueProto(static_cast<int32_t>(value), message);
  }
  if (full_name == "google.protobuf.Int64Value") {
    return WrapDynamicInt64ValueProto(value, message);
  }
  auto status = absl::StrCat(full_name, " is not int-like");
  ABSL_DLOG(FATAL) << status;
  return absl::InvalidArgumentError(status);
}

absl::Status WrapDynamicUnsignedIntegralValueProto(uint64_t value,
                                                   google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  const auto& full_name = desc->full_name();
  if (full_name == "google.protobuf.UInt32Value") {
    if (ABSL_PREDICT_FALSE(value > std::numeric_limits<uint32_t>::max())) {
      return absl::OutOfRangeError("uint64 out of uint32_t range");
    }
    return WrapDynamicUInt32ValueProto(static_cast<uint32_t>(value), message);
  }
  if (full_name == "google.protobuf.UInt64Value") {
    return WrapDynamicUInt64ValueProto(value, message);
  }
  auto status = absl::StrCat(full_name, " is not uint-like");
  ABSL_DLOG(FATAL) << status;
  return absl::InvalidArgumentError(status);
}

absl::Status WrapDynamicFloatingPointValueProto(double value,
                                                google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  const auto& full_name = desc->full_name();
  if (full_name == "google.protobuf.FloatValue") {
    if (ABSL_PREDICT_FALSE(!std::isnan(value) &&
                           static_cast<double>(static_cast<float>(value)) !=
                               value)) {
      return absl::OutOfRangeError("double out of float range");
    }
    return WrapDynamicFloatValueProto(static_cast<float>(value), message);
  }
  if (full_name == "google.protobuf.DoubleValue") {
    return WrapDynamicDoubleValueProto(value, message);
  }
  auto status = absl::StrCat(full_name, " is not double-like");
  ABSL_DLOG(FATAL) << status;
  return absl::InvalidArgumentError(status);
}

}  // namespace cel::extensions::protobuf_internal
