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

#include "extensions/protobuf/type_introspector.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_introspector.h"
#include "extensions/protobuf/type.h"
#include "internal/status_macros.h"

namespace cel::extensions {

absl::StatusOr<absl::optional<TypeView>> ProtoTypeIntrospector::FindTypeImpl(
    TypeFactory& type_factory, absl::string_view name, Type& scratch) const {
  // We do not have to worry about well known types here.
  // `TypeIntrospector::FindType` handles those directly.
  const auto* desc = descriptor_pool()->FindMessageTypeByName(name);
  if (desc == nullptr) {
    return absl::nullopt;
  }
  scratch = type_factory.CreateStructType(desc->full_name());
  return scratch;
}

absl::StatusOr<absl::optional<StructTypeFieldView>>
ProtoTypeIntrospector::FindStructTypeFieldByNameImpl(
    TypeFactory& type_factory, absl::string_view type, absl::string_view name,
    StructTypeField& scratch) const {
  // We do not have to worry about well known types here.
  // `TypeIntrospector::FindStructTypeFieldByName` handles those directly.
  const auto* desc = descriptor_pool()->FindMessageTypeByName(type);
  if (desc == nullptr) {
    return absl::nullopt;
  }
  const auto* field_desc = desc->FindFieldByName(name);
  if (field_desc == nullptr) {
    field_desc = descriptor_pool()->FindExtensionByPrintableName(desc, name);
    if (field_desc == nullptr) {
      return absl::nullopt;
    }
  }
  StructTypeFieldView result;
  CEL_ASSIGN_OR_RETURN(
      result.type,
      ProtoFieldTypeToType(type_factory, field_desc, scratch.type));
  result.name = field_desc->name();
  result.number = field_desc->number();
  return result;
}

}  // namespace cel::extensions
