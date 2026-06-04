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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/internal/field_path.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {

// Returns a descriptor for the input type name.
// Returns an error if the type name is not found.
absl::StatusOr<const google::protobuf::Descriptor*> FindMessage(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    absl::string_view type_name);

// Returns a field descriptor for the input field name.
// Returns an error if the field name is not found.
absl::StatusOr<const google::protobuf::FieldDescriptor*> FindField(
    const google::protobuf::Descriptor* descriptor, absl::string_view field_name);

// Represents the fraction of a protobuf type's object graph that should be
// visible within CEL expressions.
class ProtoTypeMask {
 public:
  explicit ProtoTypeMask(std::string type_name,
                         const std::vector<std::string>& field_paths)
      : type_name_(std::move(type_name)) {
    for (const std::string& field_path : field_paths) {
      field_paths_.insert(FieldPath(field_path));
    }
  }

  // Returns a set of field names. The set includes the first field name from
  // each field path. We are able to return a set of absl::string_view because
  // the result is backed by the descriptor pool.
  absl::StatusOr<absl::btree_set<absl::string_view>> GetFieldNames(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool) const;

  // Returns the type's full name.
  // For example: "google.rpc.context.AttributeContext".
  absl::string_view GetTypeName() const { return type_name_; }

  // Returns a representation of the FieldMask, which is a set of field paths.
  // For example:
  // {
  //    FieldPath {
  //      field path: 'resource.name',
  //      field selection: {'resource', 'name'}
  //    },
  //    FieldPath {
  //      field path: 'request.auth.claims',
  //      field selection: {'request', 'auth', 'claims'}
  //    }
  // }
  const absl::btree_set<FieldPath>& GetFieldPaths() const {
    return field_paths_;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ProtoTypeMask& proto_type_mask) {
    sink.Append(proto_type_mask.DebugString());
  }

 private:
  std::string DebugString() const;

  // A type's full name. For example: "google.rpc.context.AttributeContext".
  std::string type_name_;
  // A representation of a FieldMask, which is a set of field paths.
  // For example:
  // {
  //    FieldPath {
  //      field path: 'resource.name',
  //      field selection: {'resource', 'name'}
  //    },
  //    FieldPath {
  //      field path: 'request.auth.claims',
  //      field selection: {'request', 'auth', 'claims'}
  //    }
  // }
  // A FieldMask contains one or more paths which contain identifier characters
  // that have been dot delimited, e.g. resource.name, request.auth.claims.
  // For each path, all descendent fields after the last element in the path are
  // visible. An empty set means all fields are hidden.
  absl::btree_set<FieldPath> field_paths_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_
