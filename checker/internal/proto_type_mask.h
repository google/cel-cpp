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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"
#include "util/gtl/iterator_adaptors.h"

class ProtoTypeMask {
 public:
  static absl::StatusOr<std::unique_ptr<ProtoTypeMask>> Create(
      const std::shared_ptr<const google::protobuf::DescriptorPool>& descriptor_pool,
      const absl::flat_hash_map<std::string, std::set<std::string>>&
          types_and_field_paths);

  absl::flat_hash_map<std::string, std::set<std::string>>
  GetTypesAndVisibleFields() {
    return types_and_visible_fields_;
  }

  // Returns true when the field name is visible. A field is visible if:
  // 1. The type name is not a key in the map.
  // 2. The type name is a key in the map and the field name is in the set of
  // field names that are visible for the type.
  bool FieldIsVisible(absl::string_view type_name,
                      absl::string_view field_name);

  static absl::StatusOr<std::set<absl::string_view>> GetFieldNames(
      const std::shared_ptr<const google::protobuf::DescriptorPool>& descriptor_pool,
      absl::string_view type_name, const std::set<std::string>& field_paths);

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ProtoTypeMask& proto_type_mask) {
    std::string new_line = "\n";
    std::string output = absl::StrCat(new_line, "ProtoTypeMask {");
    auto types_and_visible_fields = proto_type_mask.types_and_visible_fields_;
    for (absl::string_view type_name :
         gtl::key_view(types_and_visible_fields)) {
      std::set<std::string> visible_fields =
          types_and_visible_fields.find(type_name)->second;
      absl::StrAppend(
          &output, new_line, "{", new_line, " type: ", type_name, new_line,
          " fields: ", absl::StrJoin(visible_fields, ", "), new_line, "}");
    }
    absl::StrAppend(&output, new_line, "}");
    absl::Format(&sink, "%v", output);
  }

 private:
  static std::vector<std::string> Split(absl::string_view field_path);

  static absl::StatusOr<const google::protobuf::Descriptor*> FindMessage(
      const std::shared_ptr<const google::protobuf::DescriptorPool>& descriptor_pool,
      absl::string_view type_name);

  static absl::StatusOr<const google::protobuf::FieldDescriptor*> FindField(
      const google::protobuf::Descriptor* descriptor, absl::string_view field_name);

  static absl::StatusOr<const google::protobuf::Descriptor*> GetMessage(
      const google::protobuf::FieldDescriptor* field_descriptor);

  static absl::Status AddAllHiddenFields(
      absl::flat_hash_map<std::string, std::set<std::string>>&
          types_and_visible_fields,
      absl::string_view type_name);

  static absl::Status AddVisibleField(
      absl::flat_hash_map<std::string, std::set<std::string>>&
          types_and_visible_fields,
      absl::string_view type_name, absl::string_view field_name);

  static absl::StatusOr<absl::flat_hash_map<std::string, std::set<std::string>>>
  ComputeVisibleFieldsMap(
      const std::shared_ptr<const google::protobuf::DescriptorPool>& descriptor_pool,
      const absl::flat_hash_map<std::string, std::set<std::string>>&
          types_and_field_paths);

  explicit ProtoTypeMask(absl::flat_hash_map<std::string, std::set<std::string>>
                             types_and_visible_fields)
      : types_and_visible_fields_(std::move(types_and_visible_fields)) {};

  static inline constexpr char kPathDelimiter = '.';

  // Map of types that have a field mask where the keys are
  // fully qualified type names and the values are the set of field names that
  // are visible for the type.
  absl::flat_hash_map<std::string, std::set<std::string>>
      types_and_visible_fields_;
};

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_
