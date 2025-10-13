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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_REGISTRY_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "checker/internal/proto_type_mask.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {

// Returns a set of field names for the input proto type mask.
// The set includes the first field name from each field path.
absl::StatusOr<absl::flat_hash_set<absl::string_view>> GetFieldNames(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    const ProtoTypeMask& proto_type_mask);

// Stores information related to ProtoTypeMasks. Visibility is defined per type,
// meaning that all messages of a type have the same visible fields.
class ProtoTypeMaskRegistry {
 public:
  // Processes the input proto type masks to create
  static absl::StatusOr<ProtoTypeMaskRegistry> Create(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      const std::vector<ProtoTypeMask>& proto_type_masks);

  const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
  GetTypesAndVisibleFields() const {
    return types_and_visible_fields_;
  }

  // Returns true when the field name is visible. A field is visible if:
  // 1. The type name is not a key in the map.
  // 2. The type name is a key in the map and the field name is in the set of
  // field names that are visible for the type.
  bool FieldIsVisible(absl::string_view type_name,
                      absl::string_view field_name);

  std::string DebugString() const {
    std::string output = "ProtoTypeMaskRegistry { ";
    for (auto& element : types_and_visible_fields_) {
      absl::StrAppend(&output, "{type: '", element.first,
                      "', visible_fields: '",
                      absl::StrJoin(element.second, "', '"), "'} ");
    }
    absl::StrAppend(&output, "}");
    return output;
  }

 private:
  explicit ProtoTypeMaskRegistry(
      absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
          types_and_visible_fields)
      : types_and_visible_fields_(std::move(types_and_visible_fields)) {}

  // Map of types that have a field mask where the keys are
  // fully qualified type names and the values are the set of field names that
  // are visible for the type.
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
      types_and_visible_fields_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_REGISTRY_H_
