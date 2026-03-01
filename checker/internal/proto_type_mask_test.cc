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

#include "checker/internal/proto_type_mask.h"

#include <set>
#include <string>

#include "checker/internal/field_path.h"
#include "internal/testing.h"

namespace cel::checker_internal {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(ProtoTypeMaskTest, EmptyTypeNameAndEmptyFieldPathsSucceeds) {
  std::string type_name = "";
  std::set<std::string> field_paths;
  ProtoTypeMask proto_type_mask(type_name, field_paths);
  EXPECT_EQ(proto_type_mask.GetTypeName(), "");
  EXPECT_THAT(proto_type_mask.GetFieldPaths(), IsEmpty());
}

TEST(ProtoTypeMaskTest, NotEmptyTypeNameAndNotEmptyFieldPathsSucceeds) {
  std::string type_name = "google.type.Expr";
  std::set<std::string> field_paths = {"resource.name", "resource.type"};
  ProtoTypeMask proto_type_mask(type_name, field_paths);
  EXPECT_EQ(proto_type_mask.GetTypeName(), "google.type.Expr");
  EXPECT_THAT(proto_type_mask.GetFieldPaths(),
              UnorderedElementsAre(FieldPath("resource.name"),
                                   FieldPath("resource.type")));
}

TEST(ProtoTypeMaskTest, DebugStringPrintsTypeNameAndFieldPaths) {
  std::string type_name = "google.type.Expr";
  std::set<std::string> field_paths = {"resource.name", "resource.type"};
  ProtoTypeMask proto_type_mask(type_name, field_paths);
  EXPECT_THAT(proto_type_mask.DebugString(),
              HasSubstr("ProtoTypeMask { type name: 'google.type.Expr', field "
                        "paths: { 'resource.name', 'resource.type' } }"));
}

}  // namespace
}  // namespace cel::checker_internal
