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
#include "testing/testrunner/runner_lib.h"

#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest-spi.h"
#include "absl/log/absl_check.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/ast_proto.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "testing/testrunner/cel_test_context.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

namespace cel::test {
namespace {

using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::CheckedExpr;

template <typename T>
T ParseTextProtoOrDie(absl::string_view text_proto) {
  T result;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text_proto, &result));
  return result;
}

absl::StatusOr<std::unique_ptr<cel::Compiler>> CreateBasicCompiler() {
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::CompilerBuilder> builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  CEL_RETURN_IF_ERROR(builder->AddLibrary(cel::StandardCompilerLibrary()));
  cel::TypeCheckerBuilder& checker_builder = builder->GetCheckerBuilder();
  CEL_RETURN_IF_ERROR(
      checker_builder.AddVariable(cel::MakeVariableDecl("x", cel::IntType())));
  CEL_RETURN_IF_ERROR(
      checker_builder.AddVariable(cel::MakeVariableDecl("y", cel::IntType())));
  return std::move(builder)->Build();
}

absl::StatusOr<std::unique_ptr<const cel::Runtime>> CreateTestRuntime() {
  CEL_ASSIGN_OR_RETURN(cel::RuntimeBuilder standard_runtime_builder,
                       cel::CreateStandardRuntimeBuilder(
                           cel::internal::GetTestingDescriptorPool(), {}));
  return std::move(standard_runtime_builder).Build();
}

absl::StatusOr<
    std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder>>
CreateTestCelExpressionBuilder() {
  auto builder = google::api::expr::runtime::CreateCelExpressionBuilder();
  CEL_RETURN_IF_ERROR(google::api::expr::runtime::RegisterBuiltinFunctions(
      builder->GetRegistry()));
  return builder;
}

class TestRunnerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Create a compiler.
    ASSERT_OK_AND_ASSIGN(compiler_, CreateBasicCompiler());
  }

 protected:
  std::unique_ptr<cel::Compiler> compiler_;
};

TEST_F(TestRunnerTest, BasicTestWithRuntimeReportsSuccess) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("{'sum': x + y, 'literal': 3}"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output {
      result_value {
        map_value {
          entries {
            key { string_value: "literal" }
            value { int64_value: 3 }
          }
          entries {
            key { string_value: "sum" }
            value { int64_value: 3 }
          }
        }
      }
    }
  )pb");
  TestRunner test_runner(CelTestContext::CreateFromRuntime(
      std::move(runtime), /*options=*/{.checked_expr = checked_expr}));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_F(TestRunnerTest, BasicTestWithRuntimeReportsFailure) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("x + y == 3"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output { result_value { bool_value: false } }
  )pb");
  TestRunner test_runner(CelTestContext::CreateFromRuntime(
      std::move(runtime), /*options=*/{.checked_expr = checked_expr}));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "bool_value: true");  // Expected true; Got false
}

TEST_F(TestRunnerTest, BasicTestWithBuilderReportsSuccess) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("{'sum': x + y, 'literal': 3}"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a builder.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder> builder,
      CreateTestCelExpressionBuilder());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output {
      result_value {
        map_value {
          entries {
            key { string_value: "literal" }
            value { int64_value: 3 }
          }
          entries {
            key { string_value: "sum" }
            value { int64_value: 3 }
          }
        }
      }
    }
  )pb");
  TestRunner test_runner(CelTestContext::CreateFromCelExpressionBuilder(
      std::move(builder), /*options=*/{.checked_expr = checked_expr}));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_F(TestRunnerTest, BasicTestWithBuilderReportsFailure) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("x + y == 3"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a builder.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder> builder,
      CreateTestCelExpressionBuilder());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output { result_value { bool_value: false } }
  )pb");
  TestRunner test_runner(CelTestContext::CreateFromCelExpressionBuilder(
      std::move(builder), /*options=*/{.checked_expr = checked_expr}));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "bool_value: true");  // Expected true; Got false
}

TEST_F(TestRunnerTest, DynamicInputAndOutputWithRuntimeReportsSuccess) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { expr: "1 + 1" }
    }
    input {
      key: "y"
      value { expr: "10 - 7" }
    }
    output { result_expr: "7 - 2" }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  TestRunner test_runner(CelTestContext::CreateFromRuntime(
      std::move(runtime),
      /*options=*/{.checked_expr = checked_expr,
                   .compiler = std::move(compiler)}));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_F(TestRunnerTest, DynamicInputAndOutputWithRuntimeReportsFailure) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { expr: "1 + 1" }
    }
    input {
      key: "y"
      value { expr: "10 - 7" }
    }
    output { result_expr: "10" }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  TestRunner test_runner(CelTestContext::CreateFromRuntime(
      std::move(runtime),
      /*options=*/{.checked_expr = checked_expr,
                   .compiler = std::move(compiler)}));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "int64_value: 5");  // Expected 5; Got 10
}

TEST_F(TestRunnerTest, DynamicInputAndOutputWithBuilderReportsSuccess) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a CelExpressionBuilder.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder> builder,
      CreateTestCelExpressionBuilder());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { expr: "1 + 1" }
    }
    input {
      key: "y"
      value { expr: "10 - 7" }
    }
    output { result_expr: "7 - 2" }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  TestRunner test_runner(CelTestContext::CreateFromCelExpressionBuilder(
      std::move(builder),
      /*options=*/{.checked_expr = checked_expr,
                   .compiler = std::move(compiler)}));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_F(TestRunnerTest, DynamicInputAndOutputWithBuilderReportsFailure) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a CelExpressionBuilder.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder> builder,
      CreateTestCelExpressionBuilder());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { expr: "1 + 1" }
    }
    input {
      key: "y"
      value { expr: "10 - 7" }
    }
    output { result_expr: "10" }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  TestRunner test_runner(CelTestContext::CreateFromCelExpressionBuilder(
      std::move(builder),
      /*options=*/{.checked_expr = checked_expr,
                   .compiler = std::move(compiler)}));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "int64_value: 5");  // Expected 5; Got 10
}

TEST_F(TestRunnerTest, DynamicInputWithoutCompilerFails) {
  const std::string expected_error =
      "INVALID_ARGUMENT: Test case uses an expression but no compiler "
      "was provided.";

  EXPECT_FATAL_FAILURE(
      {
        //  Create a compiler.
        ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                             CreateBasicCompiler());

        ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                             compiler->Compile("x + y"));
        CheckedExpr checked_expr;
        ASSERT_THAT(
            cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
            absl_testing::IsOk());

        TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
          input {
            key: "x"
            value { expr: "1 + 1" }
          }
          input {
            key: "y"
            value { value { int64_value: 2 } }
          }
          output { result_value { int64_value: 3 } }
        )pb");

        //  Create the expression builder.
        ASSERT_OK_AND_ASSIGN(auto builder, CreateTestCelExpressionBuilder());

        //  Create the TestRunner without the compiler.
        TestRunner test_runner(CelTestContext::CreateFromCelExpressionBuilder(
            std::move(builder),
            /*options=*/{.checked_expr = checked_expr}));

        test_runner.RunTest(test_case);
      },
      expected_error);
}

TEST(TestRunnerCustomCompilerTest,
     RuntimeUsesRuntimePoolToResolveCustomProtoLiteral) {
  //  Create a custom CompilerBuilder.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(cel::StandardCompilerLibrary()),
              absl_testing::IsOk());
  cel::TypeCheckerBuilder& checker_builder = builder->GetCheckerBuilder();
  ASSERT_THAT(checker_builder.AddVariable(cel::MakeVariableDecl(
                  "custom_var", cel::MessageType(TestAllTypes::descriptor()))),
              absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Compiler> compiler,
                       std::move(builder)->Build());

  //  Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile("custom_var.single_int32 == 123"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());

  //  Create a runtime configured with the testing descriptor pool.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());

  //  Define the test case. The important part is the "custom_var" input,
  //  which forces 'ResolveValue' to run on a custom type. This succeeds because
  //  the testing descriptor pool (used by CreateTestRuntime()) is configured
  //  to contain the TestAllTypes descriptor.
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "custom_var"
      value {
        value {
          object_value {
            [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
              single_int32: 123
            }
          }
        }
      }
    }
    output { result_value { bool_value: true } }
  )pb");

  TestRunner test_runner(CelTestContext::CreateFromRuntime(
      std::move(runtime), /*options=*/{.checked_expr = checked_expr}));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_F(TestRunnerTest, BasicTestWithErrorAssertion) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    output {
      eval_error {
        errors { message: "No value with name \"y\" found in Activation" }
      }
    }
  )pb");
  TestRunner test_runner(CelTestContext::CreateFromRuntime(
      std::move(runtime), /*options=*/{.checked_expr = checked_expr}));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_F(TestRunnerTest, BasicTestFailsWhenExpectingErrorButGotValue) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler_->Compile("1 + 1"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    output {
      eval_error {
        errors { message: "No value with name \"y\" found in Activation" }
      }
    }
  )pb");
  TestRunner test_runner(CelTestContext::CreateFromRuntime(
      std::move(runtime), /*options=*/{.checked_expr = checked_expr}));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "Expected error but got value");
}
}  // namespace
}  // namespace cel::test
