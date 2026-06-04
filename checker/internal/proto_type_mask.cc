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

#include "checker/internal/proto_type_mask.h"

#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "checker/internal/field_path.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FieldDescriptor;

absl::StatusOr<const Descriptor*> FindMessage(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    absl::string_view type_name) {
  const Descriptor* descriptor =
      descriptor_pool->FindMessageTypeByName(type_name);
  if (descriptor == nullptr) {
    return absl::InvalidArgumentError(
        absl::Substitute("type '$0' not found", type_name));
  }
  return descriptor;
}

absl::StatusOr<const FieldDescriptor*> FindField(const Descriptor* descriptor,
                                                 absl::string_view field_name) {
  const FieldDescriptor* field_descriptor =
      descriptor->FindFieldByName(field_name);
  if (field_descriptor == nullptr) {
    return absl::InvalidArgumentError(
        absl::Substitute("could not select field '$0' from type '$1'",
                         field_name, descriptor->full_name()));
  }
  return field_descriptor;
}

absl::StatusOr<absl::btree_set<absl::string_view>> ProtoTypeMask::GetFieldNames(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool) const {
  CEL_ASSIGN_OR_RETURN(const Descriptor* descriptor,
                       FindMessage(descriptor_pool, this->GetTypeName()));
  absl::btree_set<absl::string_view> field_names;
  for (const FieldPath& field_path : this->GetFieldPaths()) {
    std::string field_name = field_path.GetFieldName();
    CEL_ASSIGN_OR_RETURN(const FieldDescriptor* field_descriptor,
                         FindField(descriptor, field_name));
    field_names.insert(field_descriptor->name());
  }
  return field_names;
}

std::string ProtoTypeMask::DebugString() const {
  // Represent each FieldPath by its path because it is easiest to read.
  std::vector<std::string> paths;
  paths.reserve(field_paths_.size());
  for (const FieldPath& field_path : field_paths_) {
    paths.emplace_back(field_path.GetPath());
  }
  return absl::Substitute(
      "ProtoTypeMask { type name: '$0', field paths: { '$1' } }", type_name_,
      absl::StrJoin(paths, "', '"));
}

}  // namespace cel::checker_internal
