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

#include "extensions/protobuf/internal/any.h"

#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/casts.h"
#include "internal/status_macros.h"

namespace cel::extensions::protobuf_internal {

absl::Status WrapGeneratedAnyProto(absl::string_view type_url,
                                   const absl::Cord& value,
                                   google::protobuf::Any& message) {
  message.set_type_url(type_url);
  message.set_value(std::string(value));
  return absl::OkStatus();
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
  if (ABSL_PREDICT_TRUE(desc == google::protobuf::Any::descriptor())) {
    return WrapGeneratedAnyProto(
        type_url, value,
        cel::internal::down_cast<google::protobuf::Any&>(message));
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
  if (ABSL_PREDICT_FALSE(type_url_field->is_repeated() ||
                         type_url_field->is_map())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(),
                     " has unexpected type_url field cardinality: REPEATED"));
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
  if (ABSL_PREDICT_FALSE(value_field->is_repeated() || value_field->is_map())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(),
                     " has unexpected value field cardinality: REPEATED"));
  }
  reflect->SetString(&message, type_url_field, std::string(type_url));
  reflect->SetString(&message, value_field, value);
  return absl::OkStatus();
}

absl::StatusOr<Any> UnwrapGeneratedAnyProto(
    const google::protobuf::Any& message) {
  return MakeAny(std::string(message.type_url()), message.value());
}

absl::StatusOr<Any> UnwrapDynamicAnyProto(const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Any");
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (ABSL_PREDICT_TRUE(desc == google::protobuf::Any::descriptor())) {
    return UnwrapGeneratedAnyProto(
        cel::internal::down_cast<const google::protobuf::Any&>(message));
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
  if (ABSL_PREDICT_FALSE(type_url_field->is_repeated() ||
                         type_url_field->is_map())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(),
                     " has unexpected type_url field cardinality: REPEATED"));
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
  if (ABSL_PREDICT_FALSE(value_field->is_repeated() || value_field->is_map())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(),
                     " has unexpected value field cardinality: REPEATED"));
  }
  return MakeAny(reflect->GetString(message, type_url_field),
                 reflect->GetCord(message, value_field));
}

absl::StatusOr<Json> AnyToJson(ValueFactory& value_factory,
                               absl::string_view type_url,
                               const absl::Cord& value) {
  CEL_ASSIGN_OR_RETURN(
      auto type, value_factory.type_manager().ResolveType(
                     absl::StripPrefix(type_url, kTypeGoogleApisComPrefix)));
  if (ABSL_PREDICT_FALSE(!type.has_value())) {
    return absl::NotFoundError(absl::StrCat(
        "unable to deserialize google.protobuf.Any: type not found: ",
        type_url));
  }
  // Well known types require special handling, detect them.
  bool well_known_type = false;
  switch ((*type)->kind()) {
    case TypeKind::kList:
      // google.protobuf.ListValue
      //
      // The type resolver should only ever resolve to lists for
      // google.protobuf.ListValue.
      ABSL_DCHECK((*type)->As<ListType>().element()->kind() == TypeKind::kDyn);
      well_known_type = true;
      break;
    case TypeKind::kMap:
      // google.protobuf.Struct
      //
      // The type resolver should only ever resolve to maps for
      // google.protobuf.Struct.
      ABSL_DCHECK((*type)->As<MapType>().key()->kind() == TypeKind::kString &&
                  (*type)->As<MapType>().value()->kind() == TypeKind::kDyn);
      well_known_type = true;
      break;
    case TypeKind::kWrapper:
      // google.protobuf.{X}Value
      well_known_type = true;
      break;
    case TypeKind::kDyn:
      // google.protobuf.Value
      //
      // The type resolver should only ever resolve to dyn for
      // google.protobuf.Value.
      well_known_type = true;
      break;
    case TypeKind::kDuration:
      // google.protobuf.Duration
      well_known_type = true;
      break;
    case TypeKind::kTimestamp:
      // google.protobuf.Timestamp
      well_known_type = true;
      break;
    case TypeKind::kStruct: {
      auto type_name = (*type)->name();
      // Detect two well known types that are not first-class types in CEL. They
      // appear as struct.
      well_known_type = type_name == "google.protobuf.Empty" ||
                        type_name == "google.protobuf.FieldMask";
    } break;
    default:
      break;
  }
  CEL_ASSIGN_OR_RETURN(auto any,
                       (*type)->NewValueFromAny(value_factory, value));
  CEL_ASSIGN_OR_RETURN(auto json, any->ConvertToJson(value_factory));
  if (well_known_type) {
    // If it is a well known type, we only need to include two fields: "@type"
    // and "value".
    JsonObjectBuilder builder;
    builder.reserve(2);
    builder.insert_or_assign(absl::Cord("@type"), absl::Cord(type_url));
    builder.insert_or_assign(absl::Cord("value"), std::move(json));
    return std::move(builder).Build();
  }
  if (!absl::holds_alternative<JsonObject>(json)) {
    return absl::InternalError(absl::StrCat("expected ", (*type)->name(),
                                            " to convert to JSON object"));
  }
  // Update the resulting `JsonObject` with its "@type" field.
  JsonObjectBuilder builder(absl::get<JsonObject>(std::move(json)));
  builder.insert_or_assign(absl::Cord("@type"), absl::Cord(type_url));
  return std::move(builder).Build();
}

absl::StatusOr<Json> AnyToJson(ValueFactory& value_factory, const Any& any) {
  return AnyToJson(value_factory, any.type_url(), any.value());
}

absl::StatusOr<Json> GeneratedAnyProtoToJson(
    ValueFactory& value_factory, const google::protobuf::Any& message) {
  CEL_ASSIGN_OR_RETURN(auto any, UnwrapGeneratedAnyProto(message));
  return AnyToJson(value_factory, any);
}

absl::StatusOr<Json> DynamicAnyProtoToJson(ValueFactory& value_factory,
                                           const google::protobuf::Message& message) {
  CEL_ASSIGN_OR_RETURN(auto any, UnwrapDynamicAnyProto(message));
  return AnyToJson(value_factory, any);
}

}  // namespace cel::extensions::protobuf_internal
