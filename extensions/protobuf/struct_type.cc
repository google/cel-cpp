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

#include "extensions/protobuf/struct_type.h"

#include <limits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/type_manager.h"
#include "extensions/protobuf/enum_type.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

namespace {

absl::StatusOr<Handle<Type>> FieldDescriptorToTypeSingular(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return type_manager.type_factory().GetDoubleType();
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return type_manager.type_factory().GetIntType();
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return type_manager.type_factory().GetUintType();
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return type_manager.type_factory().GetBoolType();
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return type_manager.type_factory().GetStringType();
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE: {
      const auto* desc = field_desc->message_type();
      CEL_ASSIGN_OR_RETURN(auto type,
                           type_manager.ResolveType(desc->full_name()));
      if (ABSL_PREDICT_FALSE(!type)) {
        return absl::InternalError(absl::StrCat(
            "Type implementation missing for \"", desc->full_name(), "\""));
      }
      if (ABSL_PREDICT_FALSE(!(*type).Is<ProtoStructType>())) {
        // All types present in protocol buffer messages should either be
        // appropriate builtin CEL types or other protocol buffer types. In
        // theory someone could resolve this to their custom non protocol buffer
        // type, but we will not support that as we make assumptions that all
        // protocol buffer types are our protocol buffer types.
        return absl::InternalError(absl::StrCat(
            "Unexpected type implementation for \"", desc->full_name(), "\""));
      }
      return std::move(*type);
    }
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return type_manager.type_factory().GetBytesType();
    case google::protobuf::FieldDescriptor::TYPE_ENUM: {
      const auto* desc = field_desc->enum_type();
      CEL_ASSIGN_OR_RETURN(auto type,
                           type_manager.ResolveType(desc->full_name()));
      if (ABSL_PREDICT_FALSE(!type)) {
        return absl::InternalError(absl::StrCat(
            "Type implementation missing for \"", desc->full_name(), "\""));
      }
      if (ABSL_PREDICT_FALSE(!(*type).Is<ProtoEnumType>())) {
        // All types present in protocol buffer messages should either be
        // appropriate builtin CEL types or other protocol buffer types. In
        // theory someone could resolve this to their custom non protocol buffer
        // type, but we will not support that as we make assumptions that all
        // protocol buffer types are our protocol buffer types.
        return absl::InternalError(absl::StrCat(
            "Unexpected type implementation for \"", desc->full_name(), "\""));
      }
      return std::move(*type);
    }
  }
}

absl::StatusOr<Handle<Type>> FieldDescriptorToTypeRepeated(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToTypeSingular(type_manager, field_desc));
  return type_manager.type_factory().CreateListType(type);
}

absl::StatusOr<Handle<Type>> FieldDescriptorToType(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  if (field_desc->is_map()) {
    const auto* key_desc = field_desc->message_type()->map_key();
    CEL_ASSIGN_OR_RETURN(auto key_type,
                         FieldDescriptorToTypeSingular(type_manager, key_desc));
    const auto* value_desc = field_desc->message_type()->map_value();
    CEL_ASSIGN_OR_RETURN(auto value_type, FieldDescriptorToTypeSingular(
                                              type_manager, value_desc));
    return type_manager.type_factory().CreateMapType(key_type, value_type);
  }
  if (field_desc->is_repeated()) {
    return FieldDescriptorToTypeRepeated(type_manager, field_desc);
  }
  return FieldDescriptorToTypeSingular(type_manager, field_desc);
}

}  // namespace

absl::StatusOr<absl::optional<ProtoStructType::Field>>
ProtoStructType::FindFieldByName(TypeManager& type_manager,
                                 absl::string_view name) const {
  const auto* field_desc = descriptor().FindFieldByName(name);
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return absl::nullopt;
  }
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToType(type_manager, field_desc));
  return Field{field_desc->name(), field_desc->number(), std::move(type),
               field_desc};
}

absl::StatusOr<absl::optional<ProtoStructType::Field>>
ProtoStructType::FindFieldByNumber(TypeManager& type_manager,
                                   int64_t number) const {
  if (ABSL_PREDICT_FALSE(number < std::numeric_limits<int>::min() ||
                         number > std::numeric_limits<int>::max())) {
    // Treat it as not found.
    return absl::nullopt;
  }
  const auto* field_desc =
      descriptor().FindFieldByNumber(static_cast<int>(number));
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return absl::nullopt;
  }
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToType(type_manager, field_desc));
  return Field{field_desc->name(), field_desc->number(), std::move(type),
               field_desc};
}

}  // namespace cel::extensions
