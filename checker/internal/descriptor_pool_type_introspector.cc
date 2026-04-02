// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/descriptor_pool_type_introspector.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {
namespace {

// Standard implementation for field lookups.
// Avoids building a FieldTable and just checks the DescriptorPool directly.
absl::StatusOr<absl::optional<StructTypeField>>
FindStructTypeFieldByNameDirectly(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    absl::string_view type, absl::string_view name) {
  const google::protobuf::Descriptor* absl_nullable descriptor =
      descriptor_pool->FindMessageTypeByName(type);
  if (descriptor == nullptr) {
    return absl::nullopt;
  }
  const google::protobuf::FieldDescriptor* absl_nullable field =
      descriptor->FindFieldByName(name);
  if (field != nullptr) {
    return StructTypeField(MessageTypeField(field));
  }

  field = descriptor_pool->FindExtensionByPrintableName(descriptor, name);
  if (field != nullptr) {
    return StructTypeField(MessageTypeField(field));
  }
  return absl::nullopt;
}

// Standard implementation for listing fields.
// Avoids building a FieldTable and just checks the DescriptorPool directly.
absl::StatusOr<
    absl::optional<std::vector<TypeIntrospector::StructTypeFieldListing>>>
ListStructTypeFieldsDirectly(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    absl::string_view type) {
  const google::protobuf::Descriptor* absl_nullable descriptor =
      descriptor_pool->FindMessageTypeByName(type);
  if (descriptor == nullptr) {
    return absl::nullopt;
  }

  std::vector<const google::protobuf::FieldDescriptor*> extensions;
  descriptor_pool->FindAllExtensions(descriptor, &extensions);

  std::vector<TypeIntrospector::StructTypeFieldListing> fields;
  fields.reserve(descriptor->field_count() + extensions.size());

  for (int i = 0; i < descriptor->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* field = descriptor->field(i);
    fields.push_back({field->name(), StructTypeField(MessageTypeField(field))});
  }

  return fields;
}

}  // namespace

using Field = DescriptorPoolTypeIntrospector::Field;

absl::StatusOr<absl::optional<Type>>
DescriptorPoolTypeIntrospector::FindTypeImpl(absl::string_view name) const {
  const google::protobuf::Descriptor* absl_nullable descriptor =
      descriptor_pool_->FindMessageTypeByName(name);
  if (descriptor != nullptr) {
    return Type::Message(descriptor);
  }
  const google::protobuf::EnumDescriptor* absl_nullable enum_descriptor =
      descriptor_pool_->FindEnumTypeByName(name);
  if (enum_descriptor != nullptr) {
    return Type::Enum(enum_descriptor);
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<TypeIntrospector::EnumConstant>>
DescriptorPoolTypeIntrospector::FindEnumConstantImpl(
    absl::string_view type, absl::string_view value) const {
  const google::protobuf::EnumDescriptor* absl_nullable enum_descriptor =
      descriptor_pool_->FindEnumTypeByName(type);
  if (enum_descriptor != nullptr) {
    const google::protobuf::EnumValueDescriptor* absl_nullable enum_value_descriptor =
        enum_descriptor->FindValueByName(value);
    if (enum_value_descriptor == nullptr) {
      return absl::nullopt;
    }
    return EnumConstant{
        .type = Type::Enum(enum_descriptor),
        .type_full_name = enum_descriptor->full_name(),
        .value_name = enum_value_descriptor->name(),
        .number = enum_value_descriptor->number(),
    };
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<StructTypeField>>
DescriptorPoolTypeIntrospector::FindStructTypeFieldByNameImpl(
    absl::string_view type, absl::string_view name) const {
  if (!use_json_name_) {
    return FindStructTypeFieldByNameDirectly(descriptor_pool_, type, name);
  }

  const FieldTable* field_table = GetFieldTable(type);

  if (field_table == nullptr) {
    return absl::nullopt;
  }

  if (auto it = field_table->json_name_map.find(name);
      it != field_table->json_name_map.end()) {
    return field_table->fields[it->second].field;
  }

  if (auto it = field_table->extension_name_map.find(name);
      it != field_table->extension_name_map.end()) {
    return field_table->fields[it->second].field;
  }

  return absl::nullopt;
}

absl::StatusOr<
    absl::optional<std::vector<TypeIntrospector::StructTypeFieldListing>>>
DescriptorPoolTypeIntrospector::ListFieldsForStructTypeImpl(
    absl::string_view type) const {
  if (!use_json_name_) {
    return ListStructTypeFieldsDirectly(descriptor_pool_, type);
  }

  const FieldTable* field_table = GetFieldTable(type);
  if (field_table == nullptr) {
    return absl::nullopt;
  }
  std::vector<TypeIntrospector::StructTypeFieldListing> fields;
  fields.reserve(field_table->non_extensions.size());
  for (const auto& field : field_table->non_extensions) {
    fields.push_back({field.json_name, field.field});
  }
  return fields;
}

const DescriptorPoolTypeIntrospector::FieldTable*
DescriptorPoolTypeIntrospector::GetFieldTable(
    absl::string_view type_name) const {
  absl::MutexLock lock(mu_);
  if (auto it = field_tables_.find(type_name); it != field_tables_.end()) {
    return it->second.get();
  }
  if (cel::IsWellKnownMessageType(type_name)) {
    return nullptr;
  }
  const google::protobuf::Descriptor* absl_nullable descriptor =
      descriptor_pool_->FindMessageTypeByName(type_name);
  if (descriptor == nullptr) {
    return nullptr;
  }
  absl::string_view stable_type_name = descriptor->full_name();
  ABSL_DCHECK(stable_type_name == type_name);
  std::unique_ptr<FieldTable> field_table = CreateFieldTable(descriptor);
  const FieldTable* field_table_ptr = field_table.get();
  field_tables_[stable_type_name] = std::move(field_table);
  return field_table_ptr;
}

std::unique_ptr<DescriptorPoolTypeIntrospector::FieldTable>
DescriptorPoolTypeIntrospector::CreateFieldTable(
    const google::protobuf::Descriptor* absl_nonnull descriptor) const {
  ABSL_DCHECK(!IsWellKnownMessageType(descriptor));
  std::vector<Field> fields;
  absl::flat_hash_map<absl::string_view, int> json_name_map;
  absl::flat_hash_map<absl::string_view, int> field_name_map;
  absl::flat_hash_map<absl::string_view, int> extension_name_map;

  std::vector<const google::protobuf::FieldDescriptor*> extensions;
  descriptor_pool_->FindAllExtensions(descriptor, &extensions);
  fields.reserve(descriptor->field_count() + extensions.size());

  for (int i = 0; i < descriptor->field_count(); i++) {
    const google::protobuf::FieldDescriptor* field = descriptor->field(i);
    fields.push_back(Field{
        .field = StructTypeField(MessageTypeField(field)),
        .json_name = field->json_name(),
        .is_extension = false,
    });
    field_name_map[field->name()] = fields.size() - 1;
    if (use_json_name_ && !field->json_name().empty()) {
      json_name_map[field->json_name()] = fields.size() - 1;
    }
  }
  int non_extension_count = fields.size();

  for (const google::protobuf::FieldDescriptor* extension : extensions) {
    fields.push_back(Field{
        .field = StructTypeField(MessageTypeField(extension)),
        .json_name = "",
        .is_extension = true,
    });
    extension_name_map[extension->full_name()] = fields.size() - 1;
  }
  int extension_count = fields.size() - non_extension_count;
  auto result = std::make_unique<FieldTable>();
  result->descriptor = descriptor;
  result->fields = std::move(fields);
  result->non_extensions =
      absl::MakeConstSpan(result->fields).subspan(0, non_extension_count);
  result->extensions = absl::MakeConstSpan(result->fields)
                           .subspan(non_extension_count, extension_count);
  result->json_name_map = std::move(json_name_map);
  result->field_name_map = std::move(field_name_map);
  result->extension_name_map = std::move(extension_name_map);
  return result;
}

}  // namespace cel::checker_internal
