// Copyright 2025 Google LLC
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
#include "testing/testrunner/coverage_index.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/ast_proto.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::test {
namespace {

using ::absl_testing::IsOk;
using ::cel::expr::CheckedExpr;

absl::StatusOr<std::unique_ptr<const cel::Runtime>> CreateTestRuntime() {
  CEL_ASSIGN_OR_RETURN(cel::RuntimeBuilder standard_runtime_builder,
                       cel::CreateStandardRuntimeBuilder(
                           cel::internal::GetTestingDescriptorPool(), {}));
  return std::move(standard_runtime_builder).Build();
}

TEST(CoverageIndexTest, RecordCoverageWithErrorDoesNotCrash) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("x", cel::IntType())),
              IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       std::move(compiler_builder)->Build());
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile("1/x > 1"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              IsOk());

  CoverageIndex coverage_index;
  coverage_index.Init(checked_expr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  ASSERT_THAT(EnableCoverageInRuntime(*const_cast<cel::Runtime*>(runtime.get()),
                                      coverage_index),
              IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast,
                       cel::CreateAstFromCheckedExpr(checked_expr));
  ASSERT_OK_AND_ASSIGN(auto program, runtime->CreateProgram(std::move(ast)));

  cel::Activation activation;
  activation.InsertOrAssignValue("x", cel::IntValue(0));
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(cel::Value result,
                       program->Evaluate(&arena, activation));
  EXPECT_TRUE(result.IsError());
}

TEST(CoverageIndexTest, WriteLCOV) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("x", cel::BoolType())),
              IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       std::move(compiler_builder)->Build());
  const absl::string_view kSrc = R"(x ?
true :
false
)";
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile(kSrc));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              IsOk());
  checked_expr.mutable_source_info()->set_location("test.cel");

  CoverageIndex coverage_index;
  coverage_index.Init(checked_expr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  ASSERT_THAT(EnableCoverageInRuntime(*const_cast<cel::Runtime*>(runtime.get()),
                                      coverage_index),
              IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast,
                       cel::CreateAstFromCheckedExpr(checked_expr));
  ASSERT_OK_AND_ASSIGN(auto program, runtime->CreateProgram(std::move(ast)));

  cel::Activation activation;
  activation.InsertOrAssignValue("x", cel::BoolValue(true));
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(cel::Value result,
                       program->Evaluate(&arena, activation));
  EXPECT_TRUE(result.GetBool().NativeValue());

  std::string temp_file = absl::StrCat(testing::TempDir(), "/coverage.lcov");
  coverage_index.WriteLCOV(temp_file);

  std::ifstream f(temp_file);
  std::stringstream buffer;
  buffer << f.rdbuf();
  std::string content = buffer.str();

  // Verify content.
  // We expect "test.cel" to be the source file.
  EXPECT_THAT(content, testing::HasSubstr("SF:test.cel"));
  // Line 1 (x ?) should be covered.
  EXPECT_THAT(content, testing::HasSubstr("DA:1,1"));
  // Line 2 (true) should be covered.
  EXPECT_THAT(content, testing::HasSubstr("DA:2,1"));
  // Line 3 (false) should be uncovered.
  EXPECT_THAT(content, testing::HasSubstr("DA:3,0"));
  // Line 4 (empty) should not be instrumented.
  EXPECT_THAT(content, testing::Not(testing::HasSubstr("DA:4,")));
  EXPECT_THAT(content, testing::HasSubstr("end_of_record"));
}

}  // namespace
}  // namespace cel::test
