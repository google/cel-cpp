// Copyright 2026 Google LLC
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

#include "checker/internal/proto_type_mask_registry.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "checker/internal/field_path.h"
#include "checker/internal/proto_type_mask.h"
#include "common/type.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {
namespace {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FieldDescriptor;
using TypeMap =
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>;

// Returns a message type descriptor for the input field descriptor.
// Returns an error if the field is not a message type.
absl::StatusOr<const Descriptor*> GetMessage(
    const FieldDescriptor* field_descriptor) {
  cel::MessageTypeField field(field_descriptor);
  cel::Type type = field.GetType();
  absl::optional<cel::MessageType> message_type = type.AsMessage();
  if (!message_type.has_value()) {
    return absl::InvalidArgumentError(absl::Substitute(
        "field '$0' is not a message type", field_descriptor->name()));
  }
  return &(*message_type.value());
}

// Inserts the type name with an empty set into types_and_visible_fields.
// Returns an error if the type name is already present with a non-empty set.
absl::Status AddAllHiddenFields(TypeMap& types_and_visible_fields,
                                absl::string_view type_name) {
  auto result = types_and_visible_fields.find(type_name);
  if (result != types_and_visible_fields.end()) {
    if (!result->second.empty()) {
      return absl::InvalidArgumentError(
          absl::Substitute("cannot insert a proto type mask with all hidden "
                           "fields when type '$0' has already been inserted "
                           "with a proto type mask with a visible field",
                           type_name));
    }
    return absl::OkStatus();
  }
  types_and_visible_fields.insert({std::string(type_name), {}});
  return absl::OkStatus();
}

// Inserts the type name and field name into types_and_visible_fields.
// Returns an error if the type name is already present with an empty set.
absl::Status AddVisibleField(TypeMap& types_and_visible_fields,
                             absl::string_view type_name,
                             absl::string_view field_name) {
  auto result = types_and_visible_fields.find(type_name);
  if (result != types_and_visible_fields.end()) {
    if (result->second.empty()) {
      return absl::InvalidArgumentError(absl::Substitute(
          "cannot insert a proto type mask with visible "
          "field '$0' when type '$1' has already been inserted "
          "with a proto type mask with all hidden fields",
          field_name, type_name));
    }
    result->second.insert(std::string(field_name));
    return absl::OkStatus();
  }
  types_and_visible_fields.insert(
      {std::string(type_name), {std::string(field_name)}});
  return absl::OkStatus();
}

// Processes the input proto type masks to create and return the
// types_and_visible_fields map.
// Returns an error if one of the proto type masks is not valid. For example,
// if a type is not found in the descriptor pool, if a field name is not
// found, or if a field is not a message type when we are expecting it to be.
// Returns an error if there is a conflict in field visibility when
// updating the map.
absl::StatusOr<TypeMap> ComputeVisibleFieldsMap(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    const std::vector<ProtoTypeMask>& proto_type_masks) {
  TypeMap types_and_visible_fields;
  for (const ProtoTypeMask& proto_type_mask : proto_type_masks) {
    absl::string_view type_name = proto_type_mask.GetTypeName();
    CEL_ASSIGN_OR_RETURN(const Descriptor* descriptor,
                         FindMessage(descriptor_pool, type_name));
    const absl::btree_set<FieldPath>& field_paths =
        proto_type_mask.GetFieldPaths();
    if (field_paths.empty()) {
      CEL_RETURN_IF_ERROR(
          AddAllHiddenFields(types_and_visible_fields, type_name));
    }
    for (const FieldPath& field_path : field_paths) {
      const Descriptor* target_descriptor = descriptor;
      absl::Span<const std::string> field_selection =
          field_path.GetFieldSelection();
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

}  // namespace

absl::StatusOr<std::shared_ptr<ProtoTypeMaskRegistry>>
ProtoTypeMaskRegistry::Create(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    const std::vector<ProtoTypeMask>& proto_type_masks) {
  CEL_ASSIGN_OR_RETURN(
      auto types_and_visible_fields,
      ComputeVisibleFieldsMap(descriptor_pool, proto_type_masks));
  std::shared_ptr<ProtoTypeMaskRegistry> proto_type_mask_registry =
      absl::WrapUnique(new ProtoTypeMaskRegistry(types_and_visible_fields));
  return proto_type_mask_registry;
}

bool ProtoTypeMaskRegistry::FieldIsVisible(absl::string_view type_name,
                                           absl::string_view field_name) const {
  auto iterator = types_and_visible_fields_.find(type_name);
  if (iterator != types_and_visible_fields_.end() &&
      !iterator->second.contains(field_name)) {
    return false;
  }
  return true;
}

std::string ProtoTypeMaskRegistry::DebugString() const {
  std::string output = "ProtoTypeMaskRegistry { ";
  for (auto& element : types_and_visible_fields_) {
    absl::StrAppend(&output, "{type: '", element.first, "', visible_fields: '",
                    absl::StrJoin(element.second, "', '"), "'} ");
  }
  absl::StrAppend(&output, "}");
  return output;
}

}  // namespace cel::checker_internal
