// Copyright 2025 Google LLC
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

#include "checker/internal/proto_type_mask_provider.h"

#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "checker/internal/field_path.h"
#include "checker/internal/proto_type_mask.h"
#include "common/type.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FieldDescriptor;
using TypeMap = absl::flat_hash_map<std::string, std::set<std::string>>;

absl::StatusOr<std::unique_ptr<ProtoTypeMaskProvider>>
ProtoTypeMaskProvider::Create(
    const std::shared_ptr<const DescriptorPool>& descriptor_pool,
    const std::vector<ProtoTypeMask>& proto_type_masks) {
  if (descriptor_pool == nullptr) {
    return absl::InvalidArgumentError(
        "ProtoTypeMaskProvider descriptor pool cannot be nullptr");
  }
  CEL_ASSIGN_OR_RETURN(
      auto types_and_visible_fields,
      ComputeVisibleFieldsMap(descriptor_pool, proto_type_masks));
  std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider =
      absl::WrapUnique(new ProtoTypeMaskProvider(types_and_visible_fields));
  return proto_type_mask_provider;
}

bool ProtoTypeMaskProvider::FieldIsVisible(absl::string_view type_name,
                                           absl::string_view field_name) {
  auto iterator = types_and_visible_fields_.find(type_name);
  if (iterator != types_and_visible_fields_.end() &&
      !iterator->second.contains(field_name.data())) {
    return false;
  }
  return true;
}

absl::StatusOr<std::set<absl::string_view>>
ProtoTypeMaskProvider::GetFieldNames(
    const std::shared_ptr<const google::protobuf::DescriptorPool>& descriptor_pool,
    const ProtoTypeMask& proto_type_mask) {
  CEL_ASSIGN_OR_RETURN(
      const Descriptor* descriptor,
      FindMessage(descriptor_pool, proto_type_mask.GetTypeName()));
  std::set<absl::string_view> field_numbers;
  for (const FieldPath& field_path : proto_type_mask.GetFieldPaths()) {
    std::string field_name = field_path.GetFieldName();
    CEL_ASSIGN_OR_RETURN(const FieldDescriptor* field_descriptor,
                         FindField(descriptor, field_name));
    field_numbers.insert(field_descriptor->name());
  }
  return field_numbers;
}

absl::StatusOr<const Descriptor*> ProtoTypeMaskProvider::FindMessage(
    const std::shared_ptr<const DescriptorPool>& descriptor_pool,
    absl::string_view type_name) {
  const Descriptor* descriptor =
      descriptor_pool->FindMessageTypeByName(type_name);
  if (descriptor == nullptr) {
    return absl::InvalidArgumentError(absl::Substitute(
        "ProtoTypeMaskProvider type not found (type name: '$0')", type_name));
  }
  return descriptor;
}

absl::StatusOr<const FieldDescriptor*> ProtoTypeMaskProvider::FindField(
    const Descriptor* descriptor, absl::string_view field_name) {
  const FieldDescriptor* field_descriptor =
      descriptor->FindFieldByName(field_name);
  if (field_descriptor == nullptr) {
    return absl::InvalidArgumentError(absl::Substitute(
        "ProtoTypeMaskProvider could not select field from type "
        "(type name: '$0', field name: '$1')",
        descriptor->full_name(), field_name));
  }
  return field_descriptor;
}

absl::StatusOr<const Descriptor*> ProtoTypeMaskProvider::GetMessage(
    const FieldDescriptor* field_descriptor) {
  cel::MessageTypeField field(field_descriptor);
  cel::Type type = field.GetType();
  absl::optional<cel::MessageType> message_type = type.AsMessage();
  if (!message_type.has_value()) {
    return absl::InvalidArgumentError(
        absl::Substitute("ProtoTypeMaskProvider subfield is not a message type "
                         "(field name: '$0', type: '$1')",
                         field_descriptor->name(), type.name()));
  }
  return &(*message_type.value());
}

absl::Status ProtoTypeMaskProvider::AddAllHiddenFields(
    TypeMap& types_and_visible_fields, absl::string_view type_name) {
  auto result = types_and_visible_fields.find(type_name);
  if (result != types_and_visible_fields.end()) {
    if (!result->second.empty()) {
      return absl::InvalidArgumentError(absl::Substitute(
          "ProtoTypeMaskProvider cannot insert all hidden fields when the type "
          "has "
          "already been inserted with a visible field (type name: '$0')",
          type_name));
    }
    return absl::OkStatus();
  }
  types_and_visible_fields.insert({type_name.data(), {}});
  return absl::OkStatus();
}

absl::Status ProtoTypeMaskProvider::AddVisibleField(
    TypeMap& types_and_visible_fields, absl::string_view type_name,
    absl::string_view field_name) {
  auto result = types_and_visible_fields.find(type_name);
  if (result != types_and_visible_fields.end()) {
    if (result->second.empty()) {
      return absl::InvalidArgumentError(absl::Substitute(
          "ProtoTypeMaskProvider cannot insert a visible field when "
          "the type has already been "
          "inserted with all hidden fields (type name: "
          "'$0', field name: '$1')",
          type_name, field_name));
    }
    result->second.insert(field_name.data());
    return absl::OkStatus();
  }
  types_and_visible_fields.insert({type_name.data(), {field_name.data()}});
  return absl::OkStatus();
}

absl::StatusOr<TypeMap> ProtoTypeMaskProvider::ComputeVisibleFieldsMap(
    const std::shared_ptr<const DescriptorPool>& descriptor_pool,
    const std::vector<ProtoTypeMask>& proto_type_masks) {
  TypeMap types_and_visible_fields;
  for (const ProtoTypeMask& proto_type_mask : proto_type_masks) {
    std::string type_name = proto_type_mask.GetTypeName();
    CEL_ASSIGN_OR_RETURN(const Descriptor* descriptor,
                         FindMessage(descriptor_pool, type_name));
    std::set<FieldPath> field_paths = proto_type_mask.GetFieldPaths();
    if (field_paths.empty()) {
      CEL_RETURN_IF_ERROR(
          AddAllHiddenFields(types_and_visible_fields, type_name));
    }
    for (const FieldPath& field_path : field_paths) {
      const Descriptor* target_descriptor = descriptor;
      std::vector<std::string> field_selection = field_path.GetFieldSelection();
      for (auto iterator = field_selection.begin();
           iterator != field_selection.end(); ++iterator) {
        CEL_ASSIGN_OR_RETURN(const FieldDescriptor* field_descriptor,
                             FindField(target_descriptor, *iterator));
        CEL_RETURN_IF_ERROR(AddVisibleField(types_and_visible_fields,
                                            target_descriptor->full_name(),
                                            *iterator));
        if (std::next(iterator) != field_selection.end()) {
          CEL_ASSIGN_OR_RETURN(target_descriptor, GetMessage(field_descriptor));
        }
      }
    }
  }
  return types_and_visible_fields;
}
