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

#include "validator/ast_depth_validator.h"

#include <memory>
#include <utility>

#include "absl/log/absl_check.h"
#include "checker/type_check_issue.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "validator/validator.h"

namespace cel {
namespace {

std::unique_ptr<Compiler> CreateCompiler() {
  auto builder = NewCompilerBuilder(internal::GetSharedTestingDescriptorPool());
  ABSL_CHECK_OK(builder);
  ABSL_CHECK_OK((*builder)->AddLibrary(StandardCompilerLibrary()));
  auto compiler = (*builder)->Build();
  ABSL_CHECK_OK(compiler);
  return *std::move(compiler);
}

TEST(AstDepthValidatorTest, Basic) {
  auto compiler = CreateCompiler();
  ASSERT_OK_AND_ASSIGN(auto result, compiler->Compile("1 + 2 + 3"));

  Validator validator;
  validator.AddValidation(AstDepthValidator(10));
  auto output = validator.Validate(*result.GetAst());
  EXPECT_TRUE(output.valid);

  Validator validator2;
  validator2.AddValidation(AstDepthValidator(2));
  output = validator2.Validate(*result.GetAst());
  EXPECT_FALSE(output.valid);
  EXPECT_THAT(output.issues,
              testing::Contains(testing::Property(
                  &TypeCheckIssue::message,
                  testing::Eq("AST depth 3 exceeds maximum of 2"))));
}

TEST(AstDepthValidatorTest, Nested) {
  auto compiler = CreateCompiler();
  ASSERT_OK_AND_ASSIGN(auto result,
                       compiler->Compile("1 + (2 + (3 + (4 + 5)))"));

  Validator validator;
  validator.AddValidation(AstDepthValidator(10));
  auto output = validator.Validate(*result.GetAst());
  EXPECT_TRUE(output.valid);

  Validator validator2;
  validator2.AddValidation(AstDepthValidator(4));
  output = validator2.Validate(*result.GetAst());
  EXPECT_FALSE(output.valid);
  EXPECT_THAT(output.issues,
              testing::Contains(testing::Property(
                  &TypeCheckIssue::message,
                  testing::Eq("AST depth 5 exceeds maximum of 4"))));
}

}  // namespace
}  // namespace cel
