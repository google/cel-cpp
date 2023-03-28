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

#include "extensions/protobuf/value.h"

#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "extensions/protobuf/internal/time.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "internal/status_macros.h"

namespace cel::extensions {

namespace {

struct CreateStringValueFromProtoVisitor final {
  ValueFactory& value_factory;

  absl::StatusOr<Handle<Value>> operator()(absl::string_view value) const {
    return value_factory.CreateStringValue(value);
  }

  absl::StatusOr<Handle<Value>> operator()(absl::Cord value) const {
    return value_factory.CreateStringValue(std::move(value));
  }
};

}  // namespace

absl::StatusOr<Handle<Value>> ProtoValue::Create(ValueFactory& value_factory,
                                                 const google::protobuf::Message& value) {
  const auto* desc = value.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError("protocol buffer message missing descriptor");
  }
  const auto& type_name = desc->full_name();
  if (type_name == "google.protobuf.Duration") {
    CEL_ASSIGN_OR_RETURN(
        auto duration, protobuf_internal::AbslDurationFromDurationProto(value));
    return value_factory.CreateUncheckedDurationValue(duration);
  }
  if (type_name == "google.protobuf.Timestamp") {
    CEL_ASSIGN_OR_RETURN(auto time,
                         protobuf_internal::AbslTimeFromTimestampProto(value));
    return value_factory.CreateUncheckedTimestampValue(time);
  }
  if (type_name == "google.protobuf.BoolValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapBoolValueProto(value));
    return value_factory.CreateBoolValue(wrapped);
  }
  if (type_name == "google.protobuf.BytesValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapBytesValueProto(value));
    return value_factory.CreateBytesValue(std::move(wrapped));
  }
  if (type_name == "google.protobuf.FloatValue" ||
      type_name == "google.protobuf.DoubleValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapDoubleValueProto(value));
    return value_factory.CreateDoubleValue(wrapped);
  }
  if (type_name == "google.protobuf.Int32Value" ||
      type_name == "google.protobuf.Int64Value") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapIntValueProto(value));
    return value_factory.CreateIntValue(wrapped);
  }
  if (type_name == "google.protobuf.StringValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapStringValueProto(value));
    return absl::visit(CreateStringValueFromProtoVisitor{value_factory},
                       std::move(wrapped));
  }
  if (type_name == "google.protobuf.UInt32Value" ||
      type_name == "google.protobuf.UInt64Value") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapUIntValueProto(value));
    return value_factory.CreateUintValue(wrapped);
  }
  return ProtoStructValue::Create(value_factory, value);
}

absl::StatusOr<Handle<Value>> ProtoValue::Create(ValueFactory& value_factory,
                                                 google::protobuf::Message&& value) {
  const auto* desc = value.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError("protocol buffer message missing descriptor");
  }
  const auto& type_name = desc->full_name();
  if (type_name == "google.protobuf.Duration") {
    CEL_ASSIGN_OR_RETURN(
        auto duration, protobuf_internal::AbslDurationFromDurationProto(value));
    return value_factory.CreateUncheckedDurationValue(duration);
  }
  if (type_name == "google.protobuf.Timestamp") {
    CEL_ASSIGN_OR_RETURN(auto time,
                         protobuf_internal::AbslTimeFromTimestampProto(value));
    return value_factory.CreateUncheckedTimestampValue(time);
  }
  if (type_name == "google.protobuf.BoolValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapBoolValueProto(value));
    return value_factory.CreateBoolValue(wrapped);
  }
  if (type_name == "google.protobuf.BytesValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapBytesValueProto(value));
    return value_factory.CreateBytesValue(std::move(wrapped));
  }
  if (type_name == "google.protobuf.FloatValue" ||
      type_name == "google.protobuf.DoubleValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapDoubleValueProto(value));
    return value_factory.CreateDoubleValue(wrapped);
  }
  if (type_name == "google.protobuf.Int32Value" ||
      type_name == "google.protobuf.Int64Value") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapIntValueProto(value));
    return value_factory.CreateIntValue(wrapped);
  }
  if (type_name == "google.protobuf.StringValue") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapStringValueProto(value));
    return absl::visit(CreateStringValueFromProtoVisitor{value_factory},
                       std::move(wrapped));
  }
  if (type_name == "google.protobuf.UInt32Value" ||
      type_name == "google.protobuf.UInt64Value") {
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapUIntValueProto(value));
    return value_factory.CreateUintValue(wrapped);
  }
  return ProtoStructValue::Create(value_factory, std::move(value));
}

absl::StatusOr<Handle<Value>> ProtoValue::Create(
    ValueFactory& value_factory, const google::protobuf::EnumDescriptor& descriptor,
    int value) {
  CEL_ASSIGN_OR_RETURN(
      auto type, ProtoType::Resolve(value_factory.type_manager(), descriptor));
  switch (type->kind()) {
    case Kind::kNullType:
      // google.protobuf.NullValue is an enum, which represents JSON null.
      return value_factory.GetNullValue();
    case Kind::kEnum:
      return value_factory.CreateEnumValue(std::move(type).As<ProtoEnumType>(),
                                           value);
    default:
      ABSL_UNREACHABLE();
  }
}

}  // namespace cel::extensions
