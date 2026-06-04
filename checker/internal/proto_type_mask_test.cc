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

#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/internal/field_path.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

namespace cel::checker_internal {
namespace {

using ::absl_testing::StatusIs;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(ProtoTypeMaskTest, EmptyTypeNameAndEmptyFieldPathsSucceeds) {
  std::string type_name = "";
  std::vector<std::string> field_paths;
  ProtoTypeMask proto_type_mask(type_name, field_paths);
  EXPECT_EQ(proto_type_mask.GetTypeName(), "");
  EXPECT_THAT(proto_type_mask.GetFieldPaths(), IsEmpty());
}

TEST(ProtoTypeMaskTest, NotEmptyTypeNameAndNotEmptyFieldPathsSucceeds) {
  std::string type_name = "google.type.Expr";
  std::vector<std::string> field_paths = {"resource.name", "resource.type"};
  ProtoTypeMask proto_type_mask(type_name, field_paths);
  EXPECT_EQ(proto_type_mask.GetTypeName(), "google.type.Expr");
  EXPECT_THAT(proto_type_mask.GetFieldPaths(),
              UnorderedElementsAre(FieldPath("resource.name"),
                                   FieldPath("resource.type")));
}

TEST(ProtoTypeMaskTest, GetFieldNamesWithEmptyTypeReturnsError) {
  ProtoTypeMask proto_type_mask("", {});
  EXPECT_THAT(
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("type '' not found")));
}

TEST(ProtoTypeMaskTest, GetFieldNamesWithUnknownTypeReturnsError) {
  ProtoTypeMask proto_type_mask("com.example.UnknownType", {});
  EXPECT_THAT(
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("type 'com.example.UnknownType' not found")));
}

TEST(ProtoTypeMaskTest,
     GetFieldNamesWithEmptySetFieldPathSucceedsAndReturnsEmptySet) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes", {});
  ASSERT_OK_AND_ASSIGN(
      absl::btree_set<absl::string_view> field_names,
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()));
  EXPECT_THAT(field_names, IsEmpty());
}

TEST(ProtoTypeMaskTest, GetFieldNamesWithEmptyFieldPathReturnsError) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {""});
  EXPECT_THAT(
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("could not select field '' from type "
                         "'cel.expr.conformance.proto3.TestAllTypes'")));
}

TEST(ProtoTypeMaskTest, GetFieldNamesWithDelimiterFieldPathReturnsError) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {"single_int32", "."});
  EXPECT_THAT(
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("could not select field '' from type "
                         "'cel.expr.conformance.proto3.TestAllTypes'")));
}

TEST(ProtoTypeMaskTest, GetFieldNamesWithUnknownFieldReturnsError) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {"unknown_field"});
  EXPECT_THAT(
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("could not select field 'unknown_field' from type "
                         "'cel.expr.conformance.proto3.TestAllTypes'")));
}

TEST(ProtoTypeMaskTest,
     GetFieldNamesWithValidFieldsSucceedsAndReturnsFieldNames) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {"single_int32", "single_string"});
  ASSERT_OK_AND_ASSIGN(
      absl::btree_set<absl::string_view> field_names,
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()));
  EXPECT_THAT(field_names,
              UnorderedElementsAre("single_int32", "single_string"));
}

TEST(ProtoTypeMaskTest,
     GetFieldNamesWithValidFieldPathsSucceedsAndReturnsFieldNames) {
  ProtoTypeMask proto_type_mask(
      "cel.expr.conformance.proto3.NestedTestAllTypes",
      {"payload.standalone_message.bb", "payload.single_int32",
       "child.any_field_name"});
  ASSERT_OK_AND_ASSIGN(
      absl::btree_set<absl::string_view> field_names,
      proto_type_mask.GetFieldNames(GetSharedTestingDescriptorPool().get()));
  EXPECT_THAT(field_names, UnorderedElementsAre("payload", "child"));
}

TEST(ProtoTypeMaskTest, AbslStringifyPrintsTypeNameAndFieldPaths) {
  std::string type_name = "google.type.Expr";
  std::vector<std::string> field_paths = {"resource.name", "resource.type"};
  ProtoTypeMask proto_type_mask(type_name, field_paths);
  EXPECT_THAT(absl::StrCat(proto_type_mask),
              HasSubstr("ProtoTypeMask { type name: 'google.type.Expr', field "
                        "paths: { 'resource.name', 'resource.type' } }"));
}

}  // namespace
}  // namespace cel::checker_internal
