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

#include "checker/internal/field_path.h"

#include "internal/testing.h"

namespace cel::checker_internal {
namespace {

using ::testing::ElementsAre;

TEST(FieldPathTest, EmptyPathReturnsEmptyString) {
  FieldPath field_path("");
  EXPECT_EQ(field_path.GetPath(), "");
  EXPECT_THAT(field_path.GetFieldSelection(), ElementsAre(""));
  EXPECT_EQ(field_path.GetFieldName(), "");
}

TEST(FieldPathTest, DelimiterPathReturnsEmptyStrings) {
  FieldPath field_path(".");
  EXPECT_EQ(field_path.GetPath(), ".");
  EXPECT_THAT(field_path.GetFieldSelection(), ElementsAre("", ""));
  EXPECT_EQ(field_path.GetFieldName(), "");
}

TEST(FieldPathTest, FieldPathReturnsFields) {
  FieldPath field_path("resource.name.other_field");
  EXPECT_EQ(field_path.GetPath(), "resource.name.other_field");
  EXPECT_THAT(field_path.GetFieldSelection(),
              ElementsAre("resource", "name", "other_field"));
  EXPECT_EQ(field_path.GetFieldName(), "resource");
}

TEST(FieldPathTest, DebugStringPrintsFieldSelection) {
  FieldPath field_path("resource.name");
  EXPECT_EQ(field_path.DebugString(),
            "FieldPath { field path: 'resource.name', field selection: "
            "{'resource', 'name'} }");
}

TEST(FieldPathTest, EqualsComparesFieldSelectionAndReturnsTrue) {
  FieldPath field_path_1("resource.name");
  FieldPath field_path_2("resource.name");
  EXPECT_TRUE(field_path_1 == field_path_2);
}

TEST(FieldPathTest, EqualsComparesFieldSelectionAndReturnsFalse) {
  FieldPath field_path_1("resource.name");
  FieldPath field_path_2("resource.type");
  EXPECT_FALSE(field_path_1 == field_path_2);
}

TEST(FieldPathTest, LessThanComparesFieldSelectionAndReturnsTrue) {
  FieldPath field_path_1("resource.name");
  FieldPath field_path_2("resource.type");
  EXPECT_TRUE(field_path_1 < field_path_2);
}

TEST(FieldPathTest, LessThanComparesFieldSelectionAndReturnsFalse) {
  FieldPath field_path_1("resource.name");
  FieldPath field_path_2("resource.name");
  EXPECT_FALSE(field_path_1 < field_path_2);
}

}  // namespace
}  // namespace cel::checker_internal
