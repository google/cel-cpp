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

#include "checker/internal/proto_type_mask_provider.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/internal/proto_type_mask.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

namespace {

using ::absl_testing::StatusIs;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using TypeMap = absl::flat_hash_map<std::string, std::set<std::string>>;

TEST(ProtoTypeMaskProviderTest, CreateWithEmptyDescriptorPoolReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(/*descriptor_pool=*/nullptr,
                                    proto_type_masks),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(
                   "ProtoTypeMaskProvider descriptor pool cannot be nullptr")));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithEmptyInputSucceedsAndAllFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_provider->GetTypesAndVisibleFields(), IsEmpty());
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest, CreateWithEmptyTypeReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask("", {})};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("ProtoTypeMaskProvider type not found (type name: "
                         "'')")));
}

TEST(ProtoTypeMaskProviderTest, CreateWithUnknownTypeReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("com.example.UnknownType", {})};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("ProtoTypeMaskProvider type not found (type name: "
                         "'com.example.UnknownType')")));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithEmptySetFieldPathSucceedsAndFieldsAreHidden) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_provider->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes", IsEmpty())));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithDuplicateEmptySetFieldPathSucceedsAndFieldsAreHidden) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {}),
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_provider->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes", IsEmpty())));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest, CreateWithEmptyFieldPathReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes", {""})};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "ProtoTypeMaskProvider could not select field from type (type "
              "name: "
              "'cel.expr.conformance.proto3.TestAllTypes', field name: '')")));
}

TEST(ProtoTypeMaskProviderTest, CreateWithUnknownFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask(
      "cel.expr.conformance.proto3.TestAllTypes", {"unknown_field"})};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("ProtoTypeMaskProvider could not select field from type "
                    "(type name: "
                    "'cel.expr.conformance.proto3.TestAllTypes', field name: "
                    "'unknown_field')")));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithDepthOneNonMessageFieldsSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"single_int32", "single_any", "single_timestamp"})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_provider->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("single_int32", "single_any",
                                            "single_timestamp"))));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int32"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_any"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_timestamp"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest, CreateWithDepthTwoNonMessageFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks;
  proto_type_masks.push_back(
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"single_int32.any_field_name"}));
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "ProtoTypeMaskProvider subfield is not a message type (field "
              "name: 'single_int32', type: 'int'")));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithDepthOneMessageFieldSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask(
      "cel.expr.conformance.proto3.TestAllTypes", {"standalone_message"})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(
      proto_type_mask_provider->GetTypesAndVisibleFields(),
      UnorderedElementsAre(Pair("cel.expr.conformance.proto3.TestAllTypes",
                                UnorderedElementsAre("standalone_message"))));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithDepthTwoMessageFieldSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {ProtoTypeMask(
      "cel.expr.conformance.proto3.TestAllTypes", {"standalone_message.bb"})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_provider->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("standalone_message")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                       UnorderedElementsAre("bb"))));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest, CreateWithDepthTwoUnknownFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"standalone_message.unknown_field"})};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("ProtoTypeMaskProvider could not select field from type "
                    "(type name: "
                    "'cel.expr.conformance.proto3.TestAllTypes.NestedMessage', "
                    "field name: 'unknown_field')")));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithDepthThreeMessageFieldSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.NestedTestAllTypes",
                    {"payload.standalone_message.bb"})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(proto_type_mask_provider->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.NestedTestAllTypes",
                       UnorderedElementsAre("payload")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("standalone_message")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                       UnorderedElementsAre("bb"))));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "payload"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest,
     CreateWithListOfFieldPathsSucceedsAndFieldsAreVisible) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.NestedTestAllTypes",
                    {"payload.standalone_message.bb", "payload.single_int32"})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(
      proto_type_mask_provider->GetTypesAndVisibleFields(),
      UnorderedElementsAre(
          Pair("cel.expr.conformance.proto3.NestedTestAllTypes",
               UnorderedElementsAre("payload")),
          Pair("cel.expr.conformance.proto3.TestAllTypes",
               UnorderedElementsAre("standalone_message", "single_int32")),
          Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
               UnorderedElementsAre("bb"))));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible("any_type_name",
                                                       "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "payload"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int32"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask_provider->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskProviderTest,
     CreateAddingVisibleFieldThenAllHiddenFieldsReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"standalone_message.bb"}),
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                    {})};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "ProtoTypeMaskProvider cannot insert all hidden fields when the "
              "type has already been inserted with a visible field (type name: "
              "'cel.expr.conformance.proto3.TestAllTypes.NestedMessage')")));
}

TEST(ProtoTypeMaskProviderTest,
     CreateAddingAllHiddenThenVisibleFieldReturnsError) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                    {}),
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"standalone_message.bb"})};
  EXPECT_THAT(
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "ProtoTypeMaskProvider cannot insert a visible field when the "
              "type has already been inserted with all hidden fields (type "
              "name: 'cel.expr.conformance.proto3.TestAllTypes.NestedMessage', "
              "field name: 'bb')")));
}

TEST(ProtoTypeMaskProviderTest, GetFieldNamesWithEmptyTypeReturnsError) {
  ProtoTypeMask proto_type_mask("", {});
  EXPECT_THAT(
      ProtoTypeMaskProvider::GetFieldNames(GetSharedTestingDescriptorPool(),
                                           proto_type_mask),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("ProtoTypeMaskProvider type not found (type name: "
                         "'')")));
}

TEST(ProtoTypeMaskProviderTest, GetFieldNamesWithUnknownTypeReturnsError) {
  ProtoTypeMask proto_type_mask("com.example.UnknownType", {});
  EXPECT_THAT(
      ProtoTypeMaskProvider::GetFieldNames(GetSharedTestingDescriptorPool(),
                                           proto_type_mask),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("ProtoTypeMaskProvider type not found (type name: "
                         "'com.example.UnknownType')")));
}

TEST(ProtoTypeMaskProviderTest,
     GetFieldNamesWithEmptySetFieldPathSucceedsAndReturnsEmptySet) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes", {});
  ASSERT_OK_AND_ASSIGN(std::set<absl::string_view> field_names,
                       ProtoTypeMaskProvider::GetFieldNames(
                           GetSharedTestingDescriptorPool(), proto_type_mask));
  EXPECT_THAT(field_names, IsEmpty());
}

TEST(ProtoTypeMaskProviderTest, GetFieldNamesWithEmptyFieldPathReturnsError) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {""});
  EXPECT_THAT(
      ProtoTypeMaskProvider::GetFieldNames(GetSharedTestingDescriptorPool(),
                                           proto_type_mask),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("ProtoTypeMaskProvider could not select field from type "
                    "(type name: "
                    "'cel.expr.conformance.proto3.TestAllTypes', field name: "
                    "'')")));
}

TEST(ProtoTypeMaskProviderTest, GetFieldNamesWithUnknownFieldReturnsError) {
  ProtoTypeMask proto_type_mask("cel.expr.conformance.proto3.TestAllTypes",
                                {"unknown_field"});
  EXPECT_THAT(
      ProtoTypeMaskProvider::GetFieldNames(GetSharedTestingDescriptorPool(),
                                           proto_type_mask),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("ProtoTypeMaskProvider could not select field from type "
                    "(type name: "
                    "'cel.expr.conformance.proto3.TestAllTypes', field name: "
                    "'unknown_field')")));
}

TEST(ProtoTypeMaskProviderTest,
     GetFieldNamesWithListOfFieldPathsSucceedsAndReturnsFieldNames) {
  ProtoTypeMask proto_type_mask(
      "cel.expr.conformance.proto3.NestedTestAllTypes",
      {"payload.standalone_message.bb", "payload.single_int32",
       "child.any_field_name"});
  ASSERT_OK_AND_ASSIGN(std::set<absl::string_view> field_names,
                       ProtoTypeMaskProvider::GetFieldNames(
                           GetSharedTestingDescriptorPool(), proto_type_mask));
  EXPECT_THAT(field_names, UnorderedElementsAre("payload", "child"));
}

TEST(ProtoTypeMaskProviderTest, AbslStringifyPrintsTypesAndVisibleFieldsMap) {
  std::vector<ProtoTypeMask> proto_type_masks = {
      ProtoTypeMask("cel.expr.conformance.proto3.TestAllTypes",
                    {"standalone_message.bb", "single_int32"})};
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ProtoTypeMaskProvider> proto_type_mask_provider,
      ProtoTypeMaskProvider::Create(GetSharedTestingDescriptorPool(),
                                    proto_type_masks));
  EXPECT_THAT(absl::StrCat(*proto_type_mask_provider),
              AllOf(HasSubstr("ProtoTypeMaskProvider {"), HasSubstr(R"(
{
 type: cel.expr.conformance.proto3.TestAllTypes
 visible fields: single_int32, standalone_message
})"),
                    HasSubstr(R"(
{
 type: cel.expr.conformance.proto3.TestAllTypes.NestedMessage
 visible fields: bb
})")));
}
}  // namespace
