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

#include <set>
#include <string>
#include <utility>

#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "checker/internal/field_path.h"

// Represents the fraction of a protobuf type's object graph that should be
// visible within CEL expressions.
class ProtoTypeMask {
 public:
  explicit ProtoTypeMask(std::string type_name,
                         const std::set<std::string>& field_paths)
      : type_name_(std::move(type_name)) {
    for (const std::string& field_path : field_paths) {
      field_paths_.insert(FieldPath(field_path));
    }
  };

  std::string GetTypeName() const { return type_name_; }

  std::set<FieldPath> GetFieldPaths() const { return field_paths_; }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ProtoTypeMask& proto_type_mask) {
    std::string field_paths =
        absl::StrJoin(proto_type_mask.GetFieldPaths(), ", ");
    std::string output = absl::Substitute(
        "ProtoTypeMask { type name: '$0', field paths: { $1 } }",
        proto_type_mask.GetTypeName(), field_paths);
    absl::Format(&sink, "%v", output);
  }

 private:
  // A type's full name. For example: "google.rpc.context.AttributeContext".
  std::string type_name_;
  // A representation of a FieldMask, which is a set of field paths.
  // A FieldMask contains one or more paths which contain identifier characters
  // that have been dot delimited, e.g. resource.name, request.auth.claims.
  // For each path, all descendent fields after the last element in the path are
  // visible. An empty set means all fields are hidden.
  std::set<FieldPath> field_paths_;
};

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_
