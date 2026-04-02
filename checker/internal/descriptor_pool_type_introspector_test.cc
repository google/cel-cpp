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

#include "checker/internal/descriptor_pool_type_introspector.h"

#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

namespace cel::checker_internal {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::Truly;

TEST(DescriptorPoolTypeIntrospectorTest, FindType) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());

  EXPECT_THAT(introspector.FindType("cel.expr.conformance.proto3.TestAllTypes"),
              IsOkAndHolds(Optional(Property(&Type::IsMessage, true))));
  EXPECT_THAT(introspector.FindType(
                  "cel.expr.conformance.proto3.TestAllTypes.NestedEnum"),
              IsOkAndHolds(Optional(Property(&Type::IsEnum, true))));
  EXPECT_THAT(introspector.FindType("non.existent.Type"),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST(DescriptorPoolTypeIntrospectorTest, FindEnumConstant) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());

  auto result = introspector.FindEnumConstant(
      "cel.expr.conformance.proto3.TestAllTypes.NestedEnum", "FOO");
  ASSERT_THAT(result, IsOkAndHolds(Optional(AllOf(
                          Truly([](const TypeIntrospector::EnumConstant& v) {
                            return v.value_name == "FOO" && v.number == 0;
                          })))));
}

TEST(DescriptorPoolTypeIntrospectorTest, FindStructTypeFieldByName) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());

  auto field = introspector.FindStructTypeFieldByName(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int64");
  introspector.set_use_json_name(false);

  ASSERT_THAT(field,
              IsOkAndHolds(Optional(Property(&StructTypeField::GetType,
                                             Property(&Type::IsInt, true)))));
}

TEST(DescriptorPoolTypeIntrospectorTest,
     FindStructTypeFieldByNameJsonNameIgnored) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());
  introspector.set_use_json_name(false);

  auto field = introspector.FindStructTypeFieldByName(
      "cel.expr.conformance.proto3.TestAllTypes", "singleInt64");

  EXPECT_THAT(field, IsOkAndHolds(Eq(absl::nullopt)));
}

TEST(DescriptorPoolTypeIntrospectorTest, FindExtension) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());

  auto field = introspector.FindStructTypeFieldByName(
      "cel.expr.conformance.proto2.TestAllTypes",
      "cel.expr.conformance.proto2.int32_ext");

  ASSERT_THAT(field,
              IsOkAndHolds(Optional(Property(&StructTypeField::GetType,
                                             Property(&Type::IsInt, true)))));
}

TEST(DescriptorPoolTypeIntrospectorTest, FindStructTypeFieldByNameWithJsonOpt) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());
  introspector.set_use_json_name(true);

  auto field = introspector.FindStructTypeFieldByName(
      "cel.expr.conformance.proto3.TestAllTypes", "single_int64");

  ASSERT_THAT(field, IsOkAndHolds(Eq(absl::nullopt)));
}

TEST(DescriptorPoolTypeIntrospectorTest,
     FindStructTypeFieldByNameWithJsonNameOpt) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());
  introspector.set_use_json_name(true);

  absl::StatusOr<absl::optional<StructTypeField>> field =
      introspector.FindStructTypeFieldByName(
          "cel.expr.conformance.proto3.TestAllTypes", "singleInt64");

  ASSERT_THAT(field,
              IsOkAndHolds(Optional(Property(&StructTypeField::GetType,
                                             Property(&Type::IsInt, true)))));
}

MATCHER_P(FieldListingIs, field_name, "") { return arg.name == field_name; }

TEST(DescriptorPoolTypeIntrospectorTest, ListFieldsForStructType) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());
  absl::StatusOr<
      absl::optional<std::vector<TypeIntrospector::StructTypeFieldListing>>>
      fields = introspector.ListFieldsForStructType(
          "cel.expr.conformance.proto3.TestAllTypes");
  ASSERT_THAT(fields, IsOkAndHolds(Optional(SizeIs(260))));
  EXPECT_THAT(*fields, Optional(Contains(FieldListingIs("single_int64"))));
}

TEST(DescriptorPoolTypeIntrospectorTest, ListFieldsForStructTypeExtensions) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());
  auto fields = introspector.ListFieldsForStructType(
      "cel.expr.conformance.proto2.TestAllTypes");
  ASSERT_THAT(fields, IsOkAndHolds(Optional(SizeIs(259))));
  EXPECT_THAT(**fields, Contains(FieldListingIs("single_int64")));
  EXPECT_THAT(
      **fields,
      Not(Contains(FieldListingIs("cel.expr.conformance.proto2.int32_ext"))));
}

TEST(DescriptorPoolTypeIntrospectorTest,
     ListFieldsForStructTypeWithJsonNameOpt) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());
  introspector.set_use_json_name(true);
  auto fields = introspector.ListFieldsForStructType(
      "cel.expr.conformance.proto3.TestAllTypes");
  ASSERT_THAT(fields, IsOkAndHolds(Optional(SizeIs(260))));
  EXPECT_THAT(**fields, Contains(FieldListingIs("singleInt64")));
  EXPECT_THAT(**fields, Not(Contains(FieldListingIs("single_int64"))));
}

TEST(DescriptorPoolTypeIntrospectorTest, ListFieldsForStructTypeNotFound) {
  DescriptorPoolTypeIntrospector introspector(
      internal::GetTestingDescriptorPool());
  auto fields = introspector.ListFieldsForStructType(
      "cel.expr.conformance.proto3.SomeOtherType");
  EXPECT_THAT(fields, IsOkAndHolds(Eq(absl::nullopt)));
}

}  // namespace
}  // namespace cel::checker_internal
