// Copyright 2024 Google LLC
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

#include "extensions/protobuf/internal/any.h"

#include <string>

#include "google/protobuf/any.pb.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "extensions/protobuf/internal/any_lite.h"
#include "extensions/protobuf/internal/is_generated_message.h"
#include "extensions/protobuf/internal/is_message_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<google::protobuf::Any> UnwrapDynamicAnyProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Any");
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if constexpr (NotMessageLite<google::protobuf::Any>) {
    if (IsGeneratedMessage(message)) {
      // Fast path.
      return UnwrapGeneratedAnyProto(
          google::protobuf::DownCastMessage<google::protobuf::Any>(message));
    }
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* type_url_field =
      desc->FindFieldByNumber(google::protobuf::Any::kTypeUrlFieldNumber);
  if (ABSL_PREDICT_FALSE(type_url_field == nullptr)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(), " missing type_url field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(type_url_field->cpp_type() !=
                         google::protobuf::FieldDescriptor::CPPTYPE_STRING)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(), " has unexpected type_url field type: ",
        type_url_field->cpp_type_name()));
  }
  if (ABSL_PREDICT_FALSE(type_url_field->is_map() ||
                         type_url_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     type_url_field->name(), " field cardinality: REPEATED"));
  }
  const auto* value_field =
      desc->FindFieldByNumber(google::protobuf::Any::kValueFieldNumber);
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
  if (ABSL_PREDICT_FALSE(value_field->is_map() || value_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     value_field->name(), " field cardinality: REPEATED"));
  }
  return MakeAny(reflect->GetString(message, type_url_field),
                 reflect->GetCord(message, value_field));
}

absl::Status WrapDynamicAnyProto(absl::string_view type_url,
                                 const absl::Cord& value,
                                 google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Any");
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if constexpr (NotMessageLite<google::protobuf::Any>) {
    if (IsGeneratedMessage(message)) {
      // Fast path.
      return WrapGeneratedAnyProto(
          type_url, value,
          google::protobuf::DownCastMessage<google::protobuf::Any>(message));
    }
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* type_url_field =
      desc->FindFieldByNumber(google::protobuf::Any::kTypeUrlFieldNumber);
  if (ABSL_PREDICT_FALSE(type_url_field == nullptr)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(), " missing type_url field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(type_url_field->cpp_type() !=
                         google::protobuf::FieldDescriptor::CPPTYPE_STRING)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(), " has unexpected type_url field type: ",
        type_url_field->cpp_type_name()));
  }
  if (ABSL_PREDICT_FALSE(type_url_field->is_map() ||
                         type_url_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     type_url_field->name(), " field cardinality: REPEATED"));
  }
  const auto* value_field =
      desc->FindFieldByNumber(google::protobuf::Any::kValueFieldNumber);
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
  if (ABSL_PREDICT_FALSE(value_field->is_map() || value_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     value_field->name(), " field cardinality: REPEATED"));
  }
  reflect->SetString(&message, type_url_field, std::string(type_url));
  reflect->SetString(&message, value_field, value);
  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal
