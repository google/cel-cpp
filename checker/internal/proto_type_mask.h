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
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "checker/internal/field_path.h"

namespace cel::checker_internal {

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

  absl::string_view GetTypeName() const { return type_name_; }

  const absl::btree_set<FieldPath>& GetFieldPaths() const {
    return field_paths_;
  }

  std::string DebugString() const {
    // Represent each FieldPath by its path because it is easiest to read.
    std::vector<std::string> paths;
    paths.reserve(field_paths_.size());
    for (const FieldPath& field_path : field_paths_) {
      paths.emplace_back(field_path.GetPath());
    }
    return absl::Substitute(
        "ProtoTypeMask { type name: '$0', field paths: { '$1' } }",
        type_name_, absl::StrJoin(paths, "', '"));
  }

 private:
  // A type's full name. For example: "google.rpc.context.AttributeContext".
  std::string type_name_;
  // A representation of a FieldMask, which is a set of field paths.
  // A FieldMask contains one or more paths which contain identifier characters
  // that have been dot delimited, e.g. resource.name, request.auth.claims.
  // For each path, all descendent fields after the last element in the path are
  // visible. An empty set means all fields are hidden.
  absl::btree_set<FieldPath> field_paths_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_PROTO_TYPE_MASK_H_
