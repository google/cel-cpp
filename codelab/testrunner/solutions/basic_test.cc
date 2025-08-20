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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/ast_proto.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/status_macros.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/function_adapter.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/cel_test_factories.h"

namespace cel::testing {

using ::cel::expr::conformance::test::TestSuite;
using ::cel::test::CelTestContext;
using ::cel::expr::CheckedExpr;

ABSL_CONST_INIT const uint8_t kTestSuite[] = {
#include "codelab/testrunner/solutions/basic_testsuite.inc"
};

// Constants for riskScore logic
constexpr int64_t kLowRiskUserId = 1L;
constexpr int64_t kHighRiskUserId = 2L;
constexpr int64_t kLowRiskScore = 1L;
constexpr int64_t kHighRiskScore = 5L;
constexpr int64_t kUnknownRiskScore = -1L;

// Programmatically provide the test suite by registering a factory function
// that returns a `::cel::expr::conformance::test::TestSuite`.
CEL_REGISTER_TEST_SUITE_FACTORY([]() {
  TestSuite testsuite;

  ABSL_CHECK(file_desc_set.ParseFromArray(  // Crash OK
     kTestSuite, ABSL_ARRAYSIZE(kTestSuite)));
  return testsuite;
});

CEL_REGISTER_TEST_CONTEXT_FACTORY(
    []() -> absl::StatusOr<std::unique_ptr<CelTestContext>> {
      // Create a compiler. Users need not create a compiler inside this method,
      // we have created this so that we are able to generate a
      // checked_expression for the purpose of testing.
      CEL_ASSIGN_OR_RETURN(
          std::unique_ptr<CompilerBuilder> builder,
          NewCompilerBuilder(internal::GetTestingDescriptorPool()));
      CEL_RETURN_IF_ERROR(builder->AddLibrary(StandardCompilerLibrary()));
      TypeCheckerBuilder& checker_builder = builder->GetCheckerBuilder();
      CEL_ASSIGN_OR_RETURN(
          FunctionDecl risk_score_func_decl,
          MakeFunctionDecl("riskScore", MakeOverloadDecl("riskScore", IntType(),
                                                         IntType())));
      CEL_RETURN_IF_ERROR(checker_builder.AddFunction(risk_score_func_decl));
      CEL_RETURN_IF_ERROR(
          checker_builder.AddVariable(MakeVariableDecl("user_id", IntType())));
      CEL_RETURN_IF_ERROR(checker_builder.AddVariable(
          MakeVariableDecl("risk_threshold", IntType())));
      CEL_RETURN_IF_ERROR(checker_builder.AddVariable(
          MakeVariableDecl("bypass_risk_check", BoolType())));
      CEL_ASSIGN_OR_RETURN(std::unique_ptr<Compiler> compiler,
                           builder->Build());

      // Compile the expression.
      CEL_ASSIGN_OR_RETURN(
          ValidationResult validation_result,
          compiler->Compile(
              "bypass_risk_check || riskScore(user_id) < risk_threshold"));
      CheckedExpr checked_expr;
      CEL_RETURN_IF_ERROR(
          AstToCheckedExpr(*validation_result.GetAst(), &checked_expr));

      // Create a runtime.
      CEL_ASSIGN_OR_RETURN(RuntimeBuilder runtime_builder,
                           CreateStandardRuntimeBuilder(
                               internal::GetTestingDescriptorPool(), {}));

      // Register the risk_score method to be available during evaluation.
      auto risk_score = [](int64_t input) {
        switch (input) {
          case kLowRiskUserId:
            return kLowRiskScore;
          case kHighRiskUserId:
            return kHighRiskScore;
          default:
            return kUnknownRiskScore;
        }
      };
      CEL_RETURN_IF_ERROR(
          (UnaryFunctionAdapter<int64_t, int64_t>::RegisterGlobalOverload(
              "riskScore", risk_score, runtime_builder.function_registry())));

      CEL_ASSIGN_OR_RETURN(std::unique_ptr<const Runtime> runtime,
                           std::move(runtime_builder).Build());

      // This example declares the environment inline for demonstration. Users
      // can import a pre-declared environment constructs.
      return test::CelTestContext::CreateFromRuntime(
          std::move(runtime),
          /*options=*/{.checked_expr = checked_expr});
    });
}  // namespace cel::testing
