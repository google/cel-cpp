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
#include "common/types/type_cache.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

namespace {

absl::StatusOr<TypeView> ProtoSingularFieldTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field_desc, Type& scratch) {
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return DoubleTypeView{};
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
      return IntTypeView{};
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      return UintTypeView{};
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return BoolTypeView{};
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return StringTypeView{};
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return ProtoTypeToType(type_factory, field_desc->message_type(), scratch);
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return BytesTypeView{};
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return ProtoEnumTypeToType(type_factory, field_desc->enum_type(),
                                 scratch);
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer message field type: ",
                       google::protobuf::FieldDescriptor::TypeName(field_desc->type())));
  }
}

}  // namespace

absl::StatusOr<TypeView> ProtoTypeToType(
    TypeFactory& type_factory, absl::Nonnull<const google::protobuf::Descriptor*> desc,
    Type& scratch) {
  switch (desc->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      return DoubleWrapperTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
      return IntWrapperTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      return UintWrapperTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      return StringWrapperTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      return BytesWrapperTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      return BoolWrapperTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
      return AnyTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION:
      return DurationTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP:
      return TimestampTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
      return DynTypeView{};
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return common_internal::ProcessLocalTypeCache::Get()->GetDynListType();
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
      return common_internal::ProcessLocalTypeCache::Get()
          ->GetStringDynMapType();
    default:
      scratch = type_factory.CreateStructType(desc->full_name());
      return scratch;
  }
}

absl::StatusOr<TypeView> ProtoEnumTypeToType(
    TypeFactory&, absl::Nonnull<const google::protobuf::EnumDescriptor*> desc, Type&) {
  if (desc->full_name() == "google.protobuf.NullValue") {
    return NullTypeView{};
  }
  return IntTypeView{};
}

absl::StatusOr<TypeView> ProtoFieldTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field_desc, Type& scratch) {
  if (field_desc->is_map()) {
    Type map_key_scratch;
    Type map_value_scratch;
    CEL_ASSIGN_OR_RETURN(
        auto key_type, ProtoFieldTypeToType(
                           type_factory, field_desc->message_type()->map_key(),
                           map_key_scratch));
    CEL_ASSIGN_OR_RETURN(
        auto value_type,
        ProtoFieldTypeToType(type_factory,
                             field_desc->message_type()->map_value(),
                             map_value_scratch));
    scratch = type_factory.CreateMapType(key_type, value_type);
    return scratch;
  }
  if (field_desc->is_repeated()) {
    Type element_scratch;
    CEL_ASSIGN_OR_RETURN(auto element_type,
                         ProtoSingularFieldTypeToType(type_factory, field_desc,
                                                      element_scratch));
    scratch = type_factory.CreateListType(element_type);
    return scratch;
  }
  return ProtoSingularFieldTypeToType(type_factory, field_desc, scratch);
}

}  // namespace cel::extensions
