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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_DESCRIPTOR_POOL_TYPE_INTROSPECTOR_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_DESCRIPTOR_POOL_TYPE_INTROSPECTOR_H_

#include <memory>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {

// Implementation of `TypeIntrospector` that uses a `google::protobuf::DescriptorPool`.
//
// This is used by the type checker to resolve protobuf types and their fields
// and apply any options like using JSON names.
//
// Neither copyable nor movable. Should be managed by a TypeCheckEnv.
class DescriptorPoolTypeIntrospector : public TypeIntrospector {
 public:
  struct Field {
    StructTypeField field;
    absl::string_view json_name;
    bool is_extension = false;
  };

  DescriptorPoolTypeIntrospector() = delete;
  explicit DescriptorPoolTypeIntrospector(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool)
      : descriptor_pool_(descriptor_pool) {}

  DescriptorPoolTypeIntrospector(const DescriptorPoolTypeIntrospector&) =
      delete;
  DescriptorPoolTypeIntrospector& operator=(
      const DescriptorPoolTypeIntrospector&) = delete;
  DescriptorPoolTypeIntrospector(DescriptorPoolTypeIntrospector&&) = delete;
  DescriptorPoolTypeIntrospector& operator=(DescriptorPoolTypeIntrospector&&) =
      delete;

  void set_use_json_name(bool use_json_name) { use_json_name_ = use_json_name; }

  bool use_json_name() const { return use_json_name_; }

 private:
  struct FieldTable {
    const google::protobuf::Descriptor* absl_nonnull descriptor;
    std::vector<Field> fields;
    absl::Span<const Field> non_extensions;
    absl::Span<const Field> extensions;
    absl::flat_hash_map<absl::string_view, int> json_name_map;
    absl::flat_hash_map<absl::string_view, int> field_name_map;
    absl::flat_hash_map<absl::string_view, int> extension_name_map;
  };

  absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      absl::string_view name) const final;

  absl::StatusOr<absl::optional<EnumConstant>> FindEnumConstantImpl(
      absl::string_view type, absl::string_view value) const final;

  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByNameImpl(
      absl::string_view type, absl::string_view name) const final;

  absl::StatusOr<absl::optional<std::vector<StructTypeFieldListing>>>
  ListFieldsForStructTypeImpl(absl::string_view type) const final;

  std::unique_ptr<FieldTable> CreateFieldTable(
      const google::protobuf::Descriptor* absl_nonnull descriptor) const;

  const FieldTable* GetFieldTable(absl::string_view type_name) const;

  // Cached map of type to field table.
  mutable absl::flat_hash_map<absl::string_view, std::unique_ptr<FieldTable>>
      field_tables_ ABSL_GUARDED_BY(mu_);

  mutable absl::Mutex mu_;
  bool use_json_name_ = false;
  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_DESCRIPTOR_POOL_TYPE_INTROSPECTOR_H_
