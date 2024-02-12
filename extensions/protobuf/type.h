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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

absl::StatusOr<TypeView> ProtoTypeToType(
    TypeFactory& type_factory, absl::Nonnull<const google::protobuf::Descriptor*> desc,
    Type& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline absl::StatusOr<Type> ProtoTypeToType(
    TypeFactory& type_factory, absl::Nonnull<const google::protobuf::Descriptor*> desc) {
  Type scratch;
  CEL_ASSIGN_OR_RETURN(auto result,
                       ProtoTypeToType(type_factory, desc, scratch));
  return Type{result};
}

absl::StatusOr<TypeView> ProtoEnumTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::EnumDescriptor*> desc,
    Type& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline absl::StatusOr<Type> ProtoEnumTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::EnumDescriptor*> desc) {
  Type scratch;
  CEL_ASSIGN_OR_RETURN(auto result,
                       ProtoEnumTypeToType(type_factory, desc, scratch));
  return Type{result};
}

absl::StatusOr<TypeView> ProtoFieldTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field_desc,
    Type& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline absl::StatusOr<Type> ProtoFieldTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field_desc) {
  Type scratch;
  CEL_ASSIGN_OR_RETURN(auto result,
                       ProtoFieldTypeToType(type_factory, field_desc, scratch));
  return Type{result};
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_
