// Copyright 2023 Google LLC
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

#include "extensions/bindings_ext.h"

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_options.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace cel::extensions {
namespace {
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::v1alpha1::SourceInfo;

using ::google::api::expr::parser::ParseWithMacros;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelFunction;
using ::google::api::expr::runtime::CelFunctionDescriptor;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::FunctionAdapter;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;

using ::google::protobuf::Arena;
using testing::HasSubstr;
using cel::internal::IsOk;
using cel::internal::StatusIs;

struct TestInfo {
  std::string expr;
  std::string err = "";
};

class TestFunction : public CelFunction {
 public:
  explicit TestFunction(absl::string_view name)
      : CelFunction(CelFunctionDescriptor(
            name, true,
            {CelValue::Type::kBool, CelValue::Type::kBool,
             CelValue::Type::kBool, CelValue::Type::kBool})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        Arena* arena) const override {
    *result = CelValue::CreateBool(true);
    return absl::OkStatus();
  }
};

// Test function used to test macro collision and non-expansion.
constexpr absl::string_view kBind = "bind";
std::unique_ptr<CelFunction> CreateBindFunction() {
  return std::make_unique<TestFunction>(kBind);
}

class BindingsExtTest : public testing::TestWithParam<TestInfo> {};

TEST_P(BindingsExtTest, EndToEnd) {
  const TestInfo& test_info = GetParam();
  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  auto result = ParseWithMacros(test_info.expr, all_macros, "<input>");
  if (!test_info.err.empty()) {
    EXPECT_THAT(result.status(), StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr(test_info.err)));
    return;
  }
  EXPECT_THAT(result, IsOk());

  ParsedExpr parsed_expr = *result;
  Expr expr = parsed_expr.expr();
  SourceInfo source_info = parsed_expr.source_info();

  // Obtain CEL Expression builder.
  InterpreterOptions options;
  options.enable_heterogeneous_equality = true;
  options.enable_empty_wrapper_null_unboxing = true;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));
  Arena arena;
  Activation activation;
  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsBool());
  EXPECT_EQ(out.BoolOrDie(), true);
}

INSTANTIATE_TEST_SUITE_P(
    CelBindingsExtTest, BindingsExtTest,
    testing::ValuesIn<TestInfo>(
        {{"cel.bind(t, true, t)"},
         {"cel.bind(msg, \"hello\", msg + msg + msg) == \"hellohellohello\""},
         {"cel.bind(t1, true, cel.bind(t2, true, t1 && t2))"},
         {"cel.bind(valid_elems, [1, 2, 3], "
          "[3, 4, 5].exists(e, e in valid_elems))"},
         {"cel.bind(valid_elems, [1, 2, 3], "
          "![4, 5].exists(e, e in valid_elems))"},
         // Testing a bound function with the same macro name, but non-cel
         // namespace. The function mirrors the macro signature, but just
         // returns true.
         {"false.bind(false, false, false)"},
         // Error case where the variable name is not a simple identifier.
         {"cel.bind(bad.name, true, bad.name)",
          "variable name must be a simple identifier"}}));

}  // namespace
}  // namespace cel::extensions
