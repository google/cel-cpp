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

#include "extensions/protobuf/type.h"

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

namespace {

absl::StatusOr<Type> ProtoSingularFieldTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field_desc) {
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return DoubleType{};
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      return IntType{};
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      return UintType{};
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return BoolType{};
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return StringType{};
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return ProtoTypeToType(type_factory, field_desc->message_type());
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return BytesType{};
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return ProtoEnumTypeToType(type_factory, field_desc->enum_type());
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer message field type: ",
                       google::protobuf::FieldDescriptor::TypeName(field_desc->type())));
  }
}

}  // namespace

absl::StatusOr<Type> ProtoTypeToType(
    TypeFactory&, absl::Nonnull<const google::protobuf::Descriptor*> desc) {
  return Type::Message(desc);
}

absl::StatusOr<Type> ProtoEnumTypeToType(
    TypeFactory&, absl::Nonnull<const google::protobuf::EnumDescriptor*> desc) {
  if (desc->full_name() == "google.protobuf.NullValue") {
    return NullType{};
  }
  return IntType{};
}

}  // namespace cel::extensions
