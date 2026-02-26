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

#include "checker/internal/proto_type_mask_registry.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "checker/internal/proto_type_mask.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

namespace cel::checker_internal {
namespace {

using ::absl_testing::StatusIs;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using TypeMap =
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>;

TEST(ProtoTypeMaskRegistryTest, GetFieldNamesWithEmptyTypeReturnsError) {
  ProtoTypeMask proto_type_mask("", {});
  EXPECT_THAT(
      GetFieldNames(GetSharedTestingDescriptorPool().get(), proto_type_mask),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("type '' not found")));
}

TEST(ProtoTypeMaskRegistryTest, GetFieldNamesWithUnknownTypeReturnsError) {
  ProtoTypeMask proto_type_mask("com.example.UnknownType", {});
  EXPECT_THAT(
      GetFieldNames(GetSharedTestingDescriptorPool().get(), proto_type_mask),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("type 'com.example.UnknownType' not found")));
}

TEST(ProtoTypeMaskRegistryTest,
     GetFieldNamesWithEmptySetFieldPathSucceedsAndReturnsEmptySet) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes", {});
  ASSERT_OK_AND_ASSIGN(
      absl::flat_hash_set<absl::string_view> field_names,
      GetFieldNames(GetSharedTestingDescriptorPool().get(), proto_type_mask));
  EXPECT_THAT(field_names, IsEmpty());
}

TEST(ProtoTypeMaskRegistryTest, GetFieldNamesWithEmptyFieldPathReturnsError) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {""});
  EXPECT_THAT(
      GetFieldNames(GetSharedTestingDescriptorPool().get(), proto_type_mask),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("could not select field '' from type "
                         "'cel.expr.conformance.proto3.TestAllTypes'")));
}

TEST(ProtoTypeMaskRegistryTest, GetFieldNamesWithUnknownFieldReturnsError) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {"unknown_field"});
  EXPECT_THAT(
      GetFieldNames(GetSharedTestingDescriptorPool().get(), proto_type_mask),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("could not select field 'unknown_field' from type "
                         "'cel.expr.conformance.proto3.TestAllTypes'")));
}

TEST(ProtoTypeMaskRegistryTest,
     GetFieldNamesWithListOfFieldPathsSucceedsAndReturnsFieldNames) {
  ProtoTypeMask proto_type_mask(
      "cel.expr.conformance.proto3.NestedTestAllTypes",
      {"payload.standalone_message.bb", "payload.single_int32",
       "child.any_field_name"});
  ASSERT_OK_AND_ASSIGN(
      absl::flat_hash_set<absl::string_view> field_names,
      GetFieldNames(GetSharedTestingDescriptorPool().get(), proto_type_mask));
  EXPECT_THAT(field_names, UnorderedElementsAre("payload", "child"));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithEmptyInputSucceedsAndAllFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_registry.GetTypesAndVisibleFields(), IsEmpty());
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest, CreateWithEmptyTypeReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask("", {})};
  EXPECT_THAT(ProtoTypeMaskRegistry::Create(
                  GetSharedTestingDescriptorPool().get(), proto_type_masks),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("type '' not found")));
}

TEST(ProtoTypeMaskRegistryTest, CreateWithUnknownTypeReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("com.example.UnknownType", {})};
  EXPECT_THAT(ProtoTypeMaskRegistry::Create(
                  GetSharedTestingDescriptorPool().get(), proto_type_masks),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("type 'com.example.UnknownType' not found")));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithEmptySetFieldPathSucceedsAndFieldsAreHidden) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_registry.GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes", IsEmpty())));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithDuplicateEmptySetFieldPathSucceedsAndFieldsAreHidden) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {}),
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_registry.GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes", IsEmpty())));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest, CreateWithEmptyFieldPathReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {""})};
  EXPECT_THAT(
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("could not select field '' from type "
                         "'cel.expr.conformance.proto3.TestAllTypes'")));
}

TEST(ProtoTypeMaskRegistryTest, CreateWithUnknownFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask(
      "cel.expr.conformance.proto3.TestAllTypes", {"unknown_field"})};
  EXPECT_THAT(
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("could not select field 'unknown_field' from type "
                         "'cel.expr.conformance.proto3.TestAllTypes'")));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithDepthOneNonMessageFieldsSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"single_int32", "single_any", "single_timestamp"})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_registry.GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("single_int32", "single_any",
                                            "single_timestamp"))));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int32"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_any"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_timestamp"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest, CreateWithDepthTwoNonMessageFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks;
  proto_type_masks.push_back(
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"single_int32.any_field_name"}));
  EXPECT_THAT(
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("field 'single_int32' is not a message type")));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithDepthOneMessageFieldSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask(
      "cel.expr.conformance.proto3.TestAllTypes", {"standalone_message"})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(
      proto_type_mask_registry.GetTypesAndVisibleFields(),
      UnorderedElementsAre(Pair("cel.expr.conformance.proto3.TestAllTypes",
                                UnorderedElementsAre("standalone_message"))));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithDepthTwoMessageFieldSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask(
      "cel.expr.conformance.proto3.TestAllTypes", {"standalone_message.bb"})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_registry.GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("standalone_message")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                       UnorderedElementsAre("bb"))));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest, CreateWithDepthTwoUnknownFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"standalone_message.unknown_field"})};
  EXPECT_THAT(
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "could not select field 'unknown_field' from type "
              "'cel.expr.conformance.proto3.TestAllTypes.NestedMessage'")));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithDepthThreeMessageFieldSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.NestedTestAllTypes",
                    {"payload.standalone_message.bb"})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_registry.GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.NestedTestAllTypes",
                       UnorderedElementsAre("payload")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("standalone_message")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                       UnorderedElementsAre("bb"))));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "payload"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateWithListOfFieldPathsSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.NestedTestAllTypes",
                    {"payload.standalone_message.bb", "payload.single_int32"})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(
      proto_type_mask_registry.GetTypesAndVisibleFields(),
      UnorderedElementsAre(
          Pair("cel.expr.conformance.proto3.NestedTestAllTypes",
               UnorderedElementsAre("payload")),
          Pair("cel.expr.conformance.proto3.TestAllTypes",
               UnorderedElementsAre("standalone_message", "single_int32")),
          Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
               UnorderedElementsAre("bb"))));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible("any_type_name",
                                                      "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "payload"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int32"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask_registry.FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateAddVisibleFieldThenAllHiddenFieldsReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"standalone_message.bb"}),
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                    {})};
  EXPECT_THAT(
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "cannot insert a proto type mask with all hidden fields when "
              "type 'cel.expr.conformance.proto3.TestAllTypes.NestedMessage' "
              "has already been inserted with a proto type mask with a visible "
              "field")));
}

TEST(ProtoTypeMaskRegistryTest,
     CreateAddAllHiddenThenVisibleFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                    {}),
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"standalone_message.bb"})};
  EXPECT_THAT(
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "cannot insert a proto type mask with visible field 'bb' when "
              "type 'cel.expr.conformance.proto3.TestAllTypes.NestedMessage' "
              "has already been inserted with a proto type mask with all "
              "hidden fields")));
}

TEST(ProtoTypeMaskRegistryTest, DebugStringPrintsTypesAndVisibleFieldsMap) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask(
      "cel.expr.conformance.proto3.TestAllTypes", {"standalone_message.bb"})};
  ASSERT_OK_AND_ASSIGN(
      ProtoTypeMaskRegistry proto_type_mask_registry,
      ProtoTypeMaskRegistry::Create(GetSharedTestingDescriptorPool().get(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_registry.DebugString(),
              HasSubstr("ProtoTypeMaskRegistry { {type: "
                        "'cel.expr.conformance.proto3.TestAllTypes', "
                        "visible_fields: 'standalone_message'} {type: "
                        "'cel.expr.conformance.proto3.TestAllTypes."
                        "NestedMessage', visible_fields: 'bb'} }"));
}
}  // namespace
}  // namespace cel::checker_internal
