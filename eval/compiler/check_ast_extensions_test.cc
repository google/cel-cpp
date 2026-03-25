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

#include "eval/compiler/check_ast_extensions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "common/ast.h"
#include "common/ast/metadata.h"
#include "common/expr.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::Ast;
using ::cel::Expr;
using ::cel::ExtensionSpec;
using ::cel::SourceInfo;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Property;
using ::testing::SizeIs;

TEST(ExtractAndValidateRuntimeExtensionsTest, EmptyExtensions) {
  Ast ast(Expr{}, SourceInfo{});
  EXPECT_THAT(ExtractAndValidateRuntimeExtensions(ast),
              IsOkAndHolds(SizeIs(0)));
}

TEST(ExtractAndValidateRuntimeExtensionsTest, FiltersNonRuntimeExtensions) {
  SourceInfo source_info;
  source_info.mutable_extensions().push_back(
      ExtensionSpec("ext1", nullptr, {ExtensionSpec::Component::kParser}));
  source_info.mutable_extensions().push_back(
      ExtensionSpec("ext2", nullptr, {ExtensionSpec::Component::kTypeChecker}));

  Ast ast(Expr(), std::move(source_info));

  EXPECT_THAT(ExtractAndValidateRuntimeExtensions(ast),
              IsOkAndHolds(SizeIs(0)));
}

TEST(ExtractAndValidateRuntimeExtensionsTest, ExtractsRuntimeExtensions) {
  SourceInfo source_info;
  source_info.mutable_extensions().push_back(
      ExtensionSpec("ext1", nullptr, {ExtensionSpec::Component::kRuntime}));
  source_info.mutable_extensions().push_back(ExtensionSpec(
      "ext2", nullptr,
      {ExtensionSpec::Component::kParser, ExtensionSpec::Component::kRuntime}));
  source_info.mutable_extensions().push_back(
      ExtensionSpec("ext3", nullptr, {ExtensionSpec::Component::kParser}));

  Ast ast(Expr(), std::move(source_info));

  auto result = ExtractAndValidateRuntimeExtensions(ast);
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, ElementsAre(Property(&ExtensionSpec::id, Eq("ext1")),
                                   Property(&ExtensionSpec::id, Eq("ext2"))));
}

TEST(ExtractAndValidateRuntimeExtensionsTest, FailsOnDuplicateRuntimeID) {
  SourceInfo source_info;
  source_info.mutable_extensions().push_back(
      ExtensionSpec("ext1", nullptr, {ExtensionSpec::Component::kRuntime}));
  source_info.mutable_extensions().push_back(ExtensionSpec(
      "ext1", nullptr,
      {ExtensionSpec::Component::kParser, ExtensionSpec::Component::kRuntime}));

  Ast ast(Expr(), std::move(source_info));

  EXPECT_THAT(ExtractAndValidateRuntimeExtensions(ast),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "duplicate extension ID: ext1"));
}

TEST(ExtractAndValidateRuntimeExtensionsTest, IgnoresDuplicateNonRuntimeID) {
  SourceInfo source_info;
  source_info.mutable_extensions().push_back(
      ExtensionSpec("ext1", nullptr, {ExtensionSpec::Component::kRuntime}));
  source_info.mutable_extensions().push_back(
      ExtensionSpec("ext1", nullptr, {ExtensionSpec::Component::kParser}));

  Ast ast(Expr(), std::move(source_info));

  auto result = ExtractAndValidateRuntimeExtensions(ast);
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, ElementsAre(Property(&ExtensionSpec::id, Eq("ext1"))));
}

}  // namespace
}  // namespace google::api::expr::runtime
