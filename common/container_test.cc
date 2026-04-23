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

#include "common/container.h"

#include "absl/status/status.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

TEST(ExpressionContainerTest, DefaultConstructed) {
  ExpressionContainer container;
  EXPECT_THAT(container.container(), IsEmpty());
  EXPECT_THAT(container.FindAlias("foo"), IsEmpty());
}

TEST(ExpressionContainerTest, MakeExpressionContainer) {
  ASSERT_OK_AND_ASSIGN(ExpressionContainer container,
                       MakeExpressionContainer("my.container"));
  EXPECT_THAT(container.container(), Eq("my.container"));

  EXPECT_THAT(MakeExpressionContainer("..invalid"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ExpressionContainerTest, MakeExpressionContainerWithAbbrevs) {
  ASSERT_OK_AND_ASSIGN(
      ExpressionContainer container,
      MakeExpressionContainer("my.container", "pkg.Abbr", "qual.pkg.Abbr2"));
  EXPECT_THAT(container.container(), Eq("my.container"));
  EXPECT_THAT(container.FindAlias("Abbr"), Eq("pkg.Abbr"));
  EXPECT_THAT(container.FindAlias("Abbr2"), Eq("qual.pkg.Abbr2"));

  EXPECT_THAT(MakeExpressionContainer("my.container", "invalid"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ExpressionContainerTest, SetContainer) {
  ExpressionContainer container;
  EXPECT_THAT(container.SetContainer("my.container.name"), IsOk());
  EXPECT_THAT(container.container(), Eq("my.container.name"));
  EXPECT_THAT(container.SetContainer("..invalid"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(container.SetContainer("foo.1invalid"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ExpressionContainerTest, AddAlias) {
  ASSERT_OK_AND_ASSIGN(ExpressionContainer container,
                       MakeExpressionContainer("my.container"));
  EXPECT_THAT(container.AddAlias("foo", "bar.baz"), IsOk());
  EXPECT_THAT(container.FindAlias("foo"), Eq("bar.baz"));
}

TEST(ExpressionContainerTest, AddAbbreviation) {
  ASSERT_OK_AND_ASSIGN(ExpressionContainer container,
                       MakeExpressionContainer("my.container"));
  EXPECT_THAT(container.AddAbbreviation("qual.pkg.TypeName"), IsOk());
  EXPECT_THAT(container.FindAlias("TypeName"), Eq("qual.pkg.TypeName"));
}

TEST(ExpressionContainerTest, ListAbbreviationsAndAliases) {
  ASSERT_OK_AND_ASSIGN(ExpressionContainer container,
                       MakeExpressionContainer("my.container"));
  EXPECT_THAT(container.AddAbbreviation("qual.pkg.Abbr"), IsOk());
  EXPECT_THAT(container.AddAlias("AliasSym", "some.long.name"), IsOk());

  EXPECT_THAT(container.ListAbbreviations(),
              UnorderedElementsAre("qual.pkg.Abbr"));

  auto aliases = container.ListAliases();
  EXPECT_THAT(aliases, SizeIs(2));
}

TEST(ExpressionContainerTest, InvalidAbbreviation) {
  ASSERT_OK_AND_ASSIGN(ExpressionContainer container,
                       MakeExpressionContainer("my.container"));
  EXPECT_THAT(container.AddAbbreviation(""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(container.AddAbbreviation("pkg"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(container.AddAbbreviation(".pkg"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(container.AddAbbreviation("pkg."),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ExpressionContainerTest, InvalidAlias) {
  ASSERT_OK_AND_ASSIGN(ExpressionContainer container,
                       MakeExpressionContainer("my.container"));
  EXPECT_THAT(container.AddAlias("", "bar"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(container.AddAlias("foo.bar", "baz"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(container.AddAlias("foo", ".baz"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ExpressionContainerTest, CollidesWithContainer) {
  ASSERT_OK_AND_ASSIGN(ExpressionContainer container,
                       MakeExpressionContainer("my.container"));
  EXPECT_THAT(container.AddAlias("my", "bar"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace cel
