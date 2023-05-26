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

#include <limits>
#include <string>

#include "google/protobuf/wrappers.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "internal/casts.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions::protobuf_internal {

namespace {

template <typename T, typename P, typename Getter>
absl::StatusOr<P> UnwrapValueProto(const google::protobuf::Message& message,
                                   google::protobuf::FieldDescriptor::CppType cpp_type,
                                   Getter&& getter) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (ABSL_PREDICT_TRUE(desc == T::descriptor())) {
    // Fast path.
    return P(cel::internal::down_cast<const T&>(message).value());
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
  return (reflect->*getter)(message, value_field);
}

template <typename T, typename V, typename P, typename Setter>
absl::Status WrapValueProto(google::protobuf::Message& message, P&& value,
                            google::protobuf::FieldDescriptor::CppType cpp_type,
                            Setter&& setter) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (ABSL_PREDICT_TRUE(desc == T::descriptor())) {
    // Fast path.
    cel::internal::down_cast<T&>(message).set_value(
        static_cast<V>(std::forward<P>(value)));
    return absl::OkStatus();
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
  (reflect->*setter)(&message, value_field, std::forward<P>(value));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<bool> UnwrapBoolValueProto(const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BoolValue");
  return UnwrapValueProto<google::protobuf::BoolValue, bool>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_BOOL,
      &google::protobuf::Reflection::GetBool);
}

absl::StatusOr<absl::Cord> UnwrapBytesValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BytesValue");
  return UnwrapValueProto<google::protobuf::BytesValue, absl::Cord>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      &google::protobuf::Reflection::GetCord);
}

absl::StatusOr<double> UnwrapFloatValueProto(const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.FloatValue");
  return UnwrapValueProto<google::protobuf::FloatValue, double>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_FLOAT,
      &google::protobuf::Reflection::GetFloat);
}

absl::StatusOr<double> UnwrapDoubleValueProto(const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc->full_name() == "google.protobuf.DoubleValue") {
    return UnwrapValueProto<google::protobuf::DoubleValue, double>(
        message, google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE,
        &google::protobuf::Reflection::GetDouble);
  }
  if (desc->full_name() == "google.protobuf.FloatValue") {
    return UnwrapFloatValueProto(message);
  }
  return absl::InvalidArgumentError(
      absl::StrCat(message.GetTypeName(), " is not double-like"));
}

absl::StatusOr<int64_t> UnwrapIntValueProto(const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc->full_name() == "google.protobuf.Int32Value") {
    return UnwrapInt32ValueProto(message);
  }
  if (desc->full_name() == "google.protobuf.Int64Value") {
    return UnwrapInt64ValueProto(message);
  }
  return absl::InvalidArgumentError(
      absl::StrCat(message.GetTypeName(), " is not int-like"));
}

absl::StatusOr<int64_t> UnwrapInt32ValueProto(const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int32Value");
  return UnwrapValueProto<google::protobuf::Int32Value, int64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_INT32,
      &google::protobuf::Reflection::GetInt32);
}

absl::StatusOr<int64_t> UnwrapInt64ValueProto(const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int64Value");
  return UnwrapValueProto<google::protobuf::Int64Value, int64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_INT64,
      &google::protobuf::Reflection::GetInt64);
}

absl::StatusOr<absl::Cord> UnwrapStringValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.StringValue");
  return UnwrapValueProto<google::protobuf::StringValue, absl::Cord>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      &google::protobuf::Reflection::GetCord);
}

absl::StatusOr<uint64_t> UnwrapUIntValueProto(const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc->full_name() == "google.protobuf.UInt32Value") {
    return UnwrapUInt32ValueProto(message);
  }
  if (desc->full_name() == "google.protobuf.UInt64Value") {
    return UnwrapUInt64ValueProto(message);
  }
  return absl::InvalidArgumentError(
      absl::StrCat(message.GetTypeName(), " is not uint-like"));
}

absl::StatusOr<uint64_t> UnwrapUInt32ValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt32Value");
  return UnwrapValueProto<google::protobuf::UInt32Value, uint64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_UINT32,
      &google::protobuf::Reflection::GetUInt32);
}

absl::StatusOr<uint64_t> UnwrapUInt64ValueProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt64Value");
  return UnwrapValueProto<google::protobuf::UInt64Value, uint64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_UINT64,
      &google::protobuf::Reflection::GetUInt64);
}

absl::Status WrapBoolValueProto(google::protobuf::Message& message, bool value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BoolValue");
  return WrapValueProto<google::protobuf::BoolValue, bool>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_BOOL,
      &google::protobuf::Reflection::SetBool);
}

absl::Status WrapBytesValueProto(google::protobuf::Message& message,
                                 const absl::Cord& value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.BytesValue");
  using Setter = void (google::protobuf::Reflection::*)(
      google::protobuf::Message*, const google::protobuf::FieldDescriptor*, const absl::Cord&)
      const;
  return WrapValueProto<google::protobuf::BytesValue, std::string>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      static_cast<Setter>(&google::protobuf::Reflection::SetString));
}

absl::Status WrapFloatValueProto(google::protobuf::Message& message, float value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.FloatValue");
  return WrapValueProto<google::protobuf::FloatValue, float>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_FLOAT,
      &google::protobuf::Reflection::SetFloat);
}

absl::Status WrapDoubleValueProto(google::protobuf::Message& message, double value) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc->full_name() == "google.protobuf.DoubleValue") {
    return WrapValueProto<google::protobuf::DoubleValue, double>(
        message, value, google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE,
        &google::protobuf::Reflection::SetDouble);
  }
  if (desc->full_name() == "google.protobuf.FloatValue") {
    if (ABSL_PREDICT_FALSE(static_cast<double>(static_cast<float>(value)) !=
                           value)) {
      return absl::OutOfRangeError("double out of float range");
    }
    return WrapFloatValueProto(message, static_cast<float>(value));
  }
  return absl::InvalidArgumentError(
      absl::StrCat(message.GetTypeName(), " is not double-like"));
}

absl::Status WrapIntValueProto(google::protobuf::Message& message, int64_t value) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc->full_name() == "google.protobuf.Int32Value") {
    if (ABSL_PREDICT_FALSE(value < std::numeric_limits<int32_t>::min() ||
                           value > std::numeric_limits<int32_t>::max())) {
      return absl::OutOfRangeError("int64 out of int32_t range");
    }
    return WrapInt32ValueProto(message, static_cast<int32_t>(value));
  }
  if (desc->full_name() == "google.protobuf.Int64Value") {
    return WrapInt64ValueProto(message, value);
  }
  return absl::InvalidArgumentError(
      absl::StrCat(message.GetTypeName(), " is not int-like"));
}

absl::Status WrapInt32ValueProto(google::protobuf::Message& message, int32_t value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int32Value");
  return WrapValueProto<google::protobuf::Int32Value, int32_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_INT32,
      &google::protobuf::Reflection::SetInt32);
}

absl::Status WrapInt64ValueProto(google::protobuf::Message& message, int64_t value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Int64Value");
  return WrapValueProto<google::protobuf::Int64Value, int64_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_INT64,
      &google::protobuf::Reflection::SetInt64);
}

absl::Status WrapUIntValueProto(google::protobuf::Message& message, uint64_t value) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc->full_name() == "google.protobuf.UInt32Value") {
    if (ABSL_PREDICT_FALSE(value > std::numeric_limits<uint32_t>::max())) {
      return absl::OutOfRangeError("uint64 out of uint32_t range");
    }
    return WrapUInt32ValueProto(message, static_cast<uint32_t>(value));
  }
  if (desc->full_name() == "google.protobuf.UInt64Value") {
    return WrapUInt64ValueProto(message, value);
  }
  return absl::InvalidArgumentError(
      absl::StrCat(message.GetTypeName(), " is not uint-like"));
}

absl::Status WrapUInt32ValueProto(google::protobuf::Message& message, uint32_t value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt32Value");
  return WrapValueProto<google::protobuf::UInt32Value, uint32_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_UINT32,
      &google::protobuf::Reflection::SetUInt32);
}

absl::Status WrapUInt64ValueProto(google::protobuf::Message& message, uint64_t value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.UInt64Value");
  return WrapValueProto<google::protobuf::UInt64Value, uint64_t>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_UINT64,
      &google::protobuf::Reflection::SetUInt64);
}

absl::Status WrapStringValueProto(google::protobuf::Message& message,
                                  const absl::Cord& value) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.StringValue");
  using Setter = void (google::protobuf::Reflection::*)(
      google::protobuf::Message*, const google::protobuf::FieldDescriptor*, const absl::Cord&)
      const;
  return WrapValueProto<google::protobuf::StringValue, std::string>(
      message, value, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      static_cast<Setter>(&google::protobuf::Reflection::SetString));
}

}  // namespace cel::extensions::protobuf_internal
