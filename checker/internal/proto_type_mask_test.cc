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

#include <memory>
#include <set>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
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

TEST(ProtoTypeMaskCreate, EmptyDescriptorPoolReturnsError) {
  TypeMap types_and_field_paths = {};
  EXPECT_THAT(
      ProtoTypeMask::Create(/*descriptor_pool=*/nullptr, types_and_field_paths),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("ProtoTypeMask descriptor pool cannot be nullptr")));
}

TEST(ProtoTypeMaskCreate, EmptyInputSucceedsAndAllFieldsAreVisible) {
  TypeMap types_and_field_paths = {};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(proto_type_mask->GetTypesAndVisibleFields(), IsEmpty());
  EXPECT_TRUE(
      proto_type_mask->FieldIsVisible("any_type_name", "any_field_name"));
}

TEST(ProtoTypeMaskCreate, EmptyTypeReturnsError) {
  TypeMap types_and_field_paths = {{"", {}}};
  EXPECT_THAT(ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                    types_and_field_paths),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("ProtoTypeMask type not found (type name: "
                                 "'')")));
}

TEST(ProtoTypeMaskCreate, UnknownTypeReturnsError) {
  TypeMap types_and_field_paths = {{"com.example.UnknownType", {}}};
  EXPECT_THAT(ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                    types_and_field_paths),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("ProtoTypeMask type not found (type name: "
                                 "'com.example.UnknownType')")));
}

TEST(ProtoTypeMaskCreate, NonMessageTypeReturnsError) {
  TypeMap types_and_field_paths = {{"com.example.UnknownType", {}}};
  EXPECT_THAT(ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                    types_and_field_paths),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("ProtoTypeMask type not found (type name: "
                                 "'com.example.UnknownType')")));
}

TEST(ProtoTypeMaskCreate, EmptySetFieldPathSucceedsAndFieldsAreHidden) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.TestAllTypes", {}}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(proto_type_mask->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes", IsEmpty())));
  EXPECT_TRUE(
      proto_type_mask->FieldIsVisible("any_type_name", "any_field_name"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskCreate, EmptyFieldPathReturnsError) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.TestAllTypes", {""}}};
  EXPECT_THAT(
      ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                            types_and_field_paths),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "ProtoTypeMask could not select field from type (type name: "
              "'cel.expr.conformance.proto3.TestAllTypes', field name: '')")));
}

TEST(ProtoTypeMaskCreate, UnknownFieldReturnsError) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.TestAllTypes", {"unknown_field"}}};
  EXPECT_THAT(
      ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                            types_and_field_paths),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(
                   "ProtoTypeMask could not select field from type (type name: "
                   "'cel.expr.conformance.proto3.TestAllTypes', field name: "
                   "'unknown_field')")));
}

TEST(ProtoTypeMaskCreate, DepthOneNonMessageFieldsSucceedsAndFieldsAreVisible) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.TestAllTypes",
       {"single_int32", "single_any", "single_timestamp"}}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(proto_type_mask->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("single_int32", "single_any",
                                            "single_timestamp"))));
  EXPECT_TRUE(
      proto_type_mask->FieldIsVisible("any_type_name", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int32"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_any"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_timestamp"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskCreate, DepthTwoNonMessageFieldReturnsError) {
  TypeMap types_and_field_paths = {{"cel.expr.conformance.proto3.TestAllTypes",
                                    {"single_int32.any_field_name"}}};
  EXPECT_THAT(
      ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                            types_and_field_paths),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("ProtoTypeMask subfield is not a message type (field "
                         "name: 'single_int32', type: 'int'")));
}

TEST(ProtoTypeMaskCreate, DepthOneMessageFieldSucceedsAndFieldsAreVisible) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.TestAllTypes", {"standalone_message"}}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(
      proto_type_mask->GetTypesAndVisibleFields(),
      UnorderedElementsAre(Pair("cel.expr.conformance.proto3.TestAllTypes",
                                UnorderedElementsAre("standalone_message"))));
  EXPECT_TRUE(
      proto_type_mask->FieldIsVisible("any_type_name", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
}

TEST(ProtoTypeMaskCreate, DepthTwoMessageFieldSucceedsAndFieldsAreVisible) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.TestAllTypes", {"standalone_message.bb"}}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(proto_type_mask->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("standalone_message")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                       UnorderedElementsAre("bb"))));
  EXPECT_TRUE(
      proto_type_mask->FieldIsVisible("any_type_name", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskCreate, DepthTwoUnknownFieldReturnsError) {
  TypeMap types_and_field_paths = {{"cel.expr.conformance.proto3.TestAllTypes",
                                    {"standalone_message.unknown_field"}}};
  EXPECT_THAT(
      ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                            types_and_field_paths),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(
                   "ProtoTypeMask could not select field from type (type name: "
                   "'cel.expr.conformance.proto3.TestAllTypes.NestedMessage', "
                   "field name: 'unknown_field')")));
}

TEST(ProtoTypeMaskCreate, DepthThreeMessageFieldSucceedsAndFieldsAreVisible) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.NestedTestAllTypes",
       {"payload.standalone_message.bb"}}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(proto_type_mask->GetTypesAndVisibleFields(),
              UnorderedElementsAre(
                  Pair("cel.expr.conformance.proto3.NestedTestAllTypes",
                       UnorderedElementsAre("payload")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes",
                       UnorderedElementsAre("standalone_message")),
                  Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
                       UnorderedElementsAre("bb"))));
  EXPECT_TRUE(
      proto_type_mask->FieldIsVisible("any_type_name", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "payload"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskCreate, ListOfFieldPathsSucceedsAndFieldsAreVisible) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.NestedTestAllTypes",
       {"payload.standalone_message.bb", "payload.single_int32"}}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(
      proto_type_mask->GetTypesAndVisibleFields(),
      UnorderedElementsAre(
          Pair("cel.expr.conformance.proto3.NestedTestAllTypes",
               UnorderedElementsAre("payload")),
          Pair("cel.expr.conformance.proto3.TestAllTypes",
               UnorderedElementsAre("standalone_message", "single_int32")),
          Pair("cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
               UnorderedElementsAre("bb"))));
  EXPECT_TRUE(
      proto_type_mask->FieldIsVisible("any_type_name", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "payload"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.NestedTestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "standalone_message"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int32"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes", "any_field_name"));
  EXPECT_TRUE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage", "bb"));
  EXPECT_FALSE(proto_type_mask->FieldIsVisible(
      "cel.expr.conformance.proto3.TestAllTypes.NestedMessage",
      "any_field_name"));
}

TEST(ProtoTypeMaskCreate, AddingVisibleFieldAndAllHiddenFieldsReturnsError) {
  TypeMap types_and_field_paths = {
      {"cel.expr.conformance.proto3.TestAllTypes", {"standalone_message.bb"}},
      {"cel.expr.conformance.proto3.TestAllTypes.NestedMessage", {}}};
  // Because the input is a hash map, there is no guaranteed order for how it
  // will be iterated through so match substrings that are common to the error
  // messages related to the type already having a visible field or already
  // having all hidden fields.
  EXPECT_THAT(
      ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                            types_and_field_paths),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ProtoTypeMask cannot insert"),
                     HasSubstr("when the type has already been inserted with"),
                     HasSubstr("visible field"), HasSubstr("all hidden fields"),
                     HasSubstr("type name: "
                               "'cel.expr.conformance.proto3.TestAllTypes."
                               "NestedMessage'"))));
}

TEST(ProtoTypeMaskGetFieldNames, EmptyTypeReturnsError) {
  absl::string_view type_name = "";
  std::set<std::string> field_paths;
  EXPECT_THAT(ProtoTypeMask::GetFieldNames(GetSharedTestingDescriptorPool(),
                                           type_name, field_paths),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("ProtoTypeMask type not found (type name: "
                                 "'')")));
}

TEST(ProtoTypeMaskGetFieldNames, UnknownTypeReturnsError) {
  absl::string_view type_name = "com.example.UnknownType";
  std::set<std::string> field_paths;
  EXPECT_THAT(ProtoTypeMask::GetFieldNames(GetSharedTestingDescriptorPool(),
                                           type_name, field_paths),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("ProtoTypeMask type not found (type name: "
                                 "'com.example.UnknownType')")));
}

TEST(ProtoTypeMaskGetFieldNames, EmptySetFieldPathSucceedsAndReturnsEmptySet) {
  absl::string_view type_name = "cel.expr.conformance.proto3.TestAllTypes";
  std::set<std::string> field_paths;
  ASSERT_OK_AND_ASSIGN(
      std::set<absl::string_view> field_names,
      ProtoTypeMask::GetFieldNames(GetSharedTestingDescriptorPool(), type_name,
                                   field_paths));
  EXPECT_THAT(field_names, IsEmpty());
}

TEST(ProtoTypeMaskGetFieldNames, EmptyFieldPathReturnsError) {
  absl::string_view type_name = "cel.expr.conformance.proto3.TestAllTypes";
  std::set<std::string> field_paths = {""};
  EXPECT_THAT(
      ProtoTypeMask::GetFieldNames(GetSharedTestingDescriptorPool(), type_name,
                                   field_paths),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(
                   "ProtoTypeMask could not select field from type (type name: "
                   "'cel.expr.conformance.proto3.TestAllTypes', field name: "
                   "'')")));
}

TEST(ProtoTypeMaskGetFieldNames, UnknownFieldReturnsError) {
  absl::string_view type_name = "cel.expr.conformance.proto3.TestAllTypes";
  std::set<std::string> field_paths = {"unknown_field"};
  EXPECT_THAT(
      ProtoTypeMask::GetFieldNames(GetSharedTestingDescriptorPool(), type_name,
                                   field_paths),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(
                   "ProtoTypeMask could not select field from type (type name: "
                   "'cel.expr.conformance.proto3.TestAllTypes', field name: "
                   "'unknown_field')")));
}

TEST(ProtoTypeMaskGetFieldNames, ListOfFieldPathsSucceedsAndReturnsFieldNames) {
  absl::string_view type_name =
      "cel.expr.conformance.proto3.NestedTestAllTypes";
  std::set<std::string> field_paths = {"payload.standalone_message.bb",
                                       "payload.single_int32",
                                       "child.any_field_name"};
  ASSERT_OK_AND_ASSIGN(
      std::set<absl::string_view> field_names,
      ProtoTypeMask::GetFieldNames(GetSharedTestingDescriptorPool(), type_name,
                                   field_paths));
  EXPECT_THAT(field_names, UnorderedElementsAre("payload", "child"));
}

TEST(ProtoTypeMaskAbslStringify, PrintsTypesAndVisibleFieldsMap) {
  TypeMap types_and_field_paths = {{"cel.expr.conformance.proto3.TestAllTypes",
                                    {"standalone_message.bb", "single_int32"}}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProtoTypeMask> proto_type_mask,
                       ProtoTypeMask::Create(GetSharedTestingDescriptorPool(),
                                             types_and_field_paths));
  EXPECT_THAT(absl::StrCat(*proto_type_mask),
              AllOf(HasSubstr("ProtoTypeMask {"), HasSubstr(R"(
{
 type: cel.expr.conformance.proto3.TestAllTypes
 fields: single_int32, standalone_message
})"),
                    HasSubstr(R"(
{
 type: cel.expr.conformance.proto3.TestAllTypes.NestedMessage
 fields: bb
})")));
}
}  // namespace
