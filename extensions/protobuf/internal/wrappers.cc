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

#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.pb.h"
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
  if (desc == T::descriptor()) {
    // Fast path.
    return cel::internal::down_cast<const T&>(message).value();
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

}  // namespace

absl::StatusOr<bool> UnwrapBoolValueProto(const google::protobuf::Message& message) {
  return UnwrapValueProto<google::protobuf::BoolValue, bool>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_BOOL,
      &google::protobuf::Reflection::GetBool);
}

absl::StatusOr<absl::Cord> UnwrapBytesValueProto(
    const google::protobuf::Message& message) {
  return UnwrapValueProto<google::protobuf::BytesValue, absl::Cord>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_STRING,
      &google::protobuf::Reflection::GetCord);
}

absl::StatusOr<double> UnwrapFloatValueProto(const google::protobuf::Message& message) {
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
  if (desc->full_name() == "google.protobuf.FloatValue") {
    return UnwrapFloatValueProto(message);
  }
  if (desc->full_name() == "google.protobuf.DoubleValue") {
    return UnwrapValueProto<google::protobuf::DoubleValue, double>(
        message, google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE,
        &google::protobuf::Reflection::GetDouble);
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
  return UnwrapValueProto<google::protobuf::Int32Value, int64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_INT32,
      &google::protobuf::Reflection::GetInt32);
}

absl::StatusOr<int64_t> UnwrapInt64ValueProto(const google::protobuf::Message& message) {
  return UnwrapValueProto<google::protobuf::Int64Value, int64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_INT64,
      &google::protobuf::Reflection::GetInt64);
}

absl::StatusOr<absl::variant<absl::string_view, absl::Cord>>
UnwrapStringValueProto(const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc == google::protobuf::StringValue::descriptor()) {
    // Fast path.
    return absl::string_view(
        cel::internal::down_cast<const google::protobuf::StringValue&>(message)
            .value());
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* value_field =
      desc->FindFieldByNumber(google::protobuf::StringValue::kValueFieldNumber);
  if (ABSL_PREDICT_FALSE(value_field == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing value field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(value_field->cpp_type() !=
                         google::protobuf::FieldDescriptor::CPPTYPE_STRING)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(),
        " has unexpected value field type: ", value_field->cpp_type_name()));
  }
  if (value_field->options().ctype() == google::protobuf::FieldOptions::CORD &&
      !value_field->is_extension()) {
    return reflect->GetCord(message, value_field);
  }
  return reflect->GetStringView(message, value_field);
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
  return UnwrapValueProto<google::protobuf::UInt32Value, uint64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_UINT32,
      &google::protobuf::Reflection::GetUInt32);
}

absl::StatusOr<uint64_t> UnwrapUInt64ValueProto(
    const google::protobuf::Message& message) {
  return UnwrapValueProto<google::protobuf::UInt64Value, uint64_t>(
      message, google::protobuf::FieldDescriptor::CPPTYPE_UINT64,
      &google::protobuf::Reflection::GetUInt64);
}

}  // namespace cel::extensions::protobuf_internal
