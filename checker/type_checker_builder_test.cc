// Copyright 2024 Google LLC
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

#include "checker/type_checker_builder.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::checker_internal::MakeTestParsedAst;
using ::testing::HasSubstr;

TEST(TypeCheckerBuilderTest, AddVariable) {
  TypeCheckerBuilder builder;

  ASSERT_THAT(builder.AddVariable(MakeVariableDecl("x", IntType())), IsOk());

  ASSERT_OK_AND_ASSIGN(auto checker, std::move(builder).Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AddVariableRedeclaredError) {
  TypeCheckerBuilder builder;

  ASSERT_THAT(builder.AddVariable(MakeVariableDecl("x", IntType())), IsOk());
  EXPECT_THAT(builder.AddVariable(MakeVariableDecl("x", IntType())),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST(TypeCheckerBuilderTest, AddFunction) {
  TypeCheckerBuilder builder;

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder.AddFunction(fn_decl), IsOk());
  ASSERT_OK_AND_ASSIGN(auto checker, std::move(builder).Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("add(1, 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AddFunctionRedeclaredError) {
  TypeCheckerBuilder builder;

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder.AddFunction(fn_decl), IsOk());
  EXPECT_THAT(builder.AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST(TypeCheckerBuilderTest, AddLibrary) {
  TypeCheckerBuilder builder;

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder.AddLibrary({"",
                                  [&](TypeCheckerBuilder& b) {
                                    return builder.AddFunction(fn_decl);
                                  }}),

              IsOk());
  ASSERT_OK_AND_ASSIGN(auto checker, std::move(builder).Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("add(1, 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AddLibraryRedeclaredError) {
  TypeCheckerBuilder builder;

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder.AddLibrary({"testlib",
                                  [&](TypeCheckerBuilder& b) {
                                    return builder.AddFunction(fn_decl);
                                  }}),
              IsOk());
  EXPECT_THAT(builder.AddLibrary({"testlib",
                                  [&](TypeCheckerBuilder& b) {
                                    return builder.AddFunction(fn_decl);
                                  }}),
              StatusIs(absl::StatusCode::kAlreadyExists, HasSubstr("testlib")));
}

TEST(TypeCheckerBuilderTest, AddLibraryForwardsErrors) {
  TypeCheckerBuilder builder;

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder.AddLibrary({"",
                                  [&](TypeCheckerBuilder& b) {
                                    return builder.AddFunction(fn_decl);
                                  }}),
              IsOk());
  EXPECT_THAT(builder.AddLibrary({"",
                                  [](TypeCheckerBuilder& b) {
                                    return absl::InternalError("test error");
                                  }}),
              StatusIs(absl::StatusCode::kInternal, HasSubstr("test error")));
}

}  // namespace
}  // namespace cel
