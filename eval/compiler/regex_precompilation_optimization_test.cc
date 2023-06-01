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

#include "eval/compiler/regex_precompilation_optimization.h"

#include <memory>
#include <utility>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "base/ast_internal.h"
#include "base/internal/ast_impl.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_options.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using cel::ast::internal::CheckedExpr;
using google::api::expr::parser::Parse;

namespace exprpb = google::api::expr::v1alpha1;

class RegexPrecompilationExtensionTest : public testing::Test {
 public:
  RegexPrecompilationExtensionTest()
      : type_registry_(*builder_.GetTypeRegistry()),
        function_registry_(*builder_.GetRegistry()),
        type_factory_(cel::MemoryManager::Global()),
        type_manager_(type_factory_, type_registry_.GetTypeProvider()),
        value_factory_(type_manager_),
        resolver_("", function_registry_.InternalGetRegistry(), &type_registry_,
                  value_factory_, type_registry_.resolveable_enums()) {
    options_.enable_regex = true;
    options_.regex_max_program_size = 100;
    options_.enable_regex_precompilation = true;
    runtime_options_ = ConvertToRuntimeOptions(options_);
  }

  void SetUp() override {
    ASSERT_OK(RegisterBuiltinFunctions(&function_registry_, options_));
  }

 protected:
  CelExpressionBuilderFlatImpl builder_;
  CelTypeRegistry& type_registry_;
  CelFunctionRegistry& function_registry_;
  InterpreterOptions options_;
  cel::RuntimeOptions runtime_options_;
  cel::TypeFactory type_factory_;
  cel::TypeManager type_manager_;
  cel::ValueFactory value_factory_;
  Resolver resolver_;
  BuilderWarnings builder_warnings_;
};

TEST_F(RegexPrecompilationExtensionTest, SmokeTest) {
  ProgramOptimizerFactory factory =
      CreateRegexPrecompilationExtension(options_.regex_max_program_size);
  ExecutionPath path;
  PlannerContext::ProgramTree program_tree;
  CheckedExpr expr;
  cel::ast::internal::AstImpl ast_impl(std::move(expr));
  PlannerContext context(resolver_, type_registry_, runtime_options_,
                         builder_warnings_, path, program_tree);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> optimizer,
                       factory(context, ast_impl));
}

MATCHER_P(ExpressionPlanSizeIs, size, "") {
  // This is brittle, but the most direct way to test that the plan
  // was optimized.
  const std::unique_ptr<CelExpression>& plan = arg;

  const CelExpressionFlatImpl* impl =
      dynamic_cast<CelExpressionFlatImpl*>(plan.get());

  if (impl == nullptr) return false;
  *result_listener << "got size " << impl->path().size();
  return impl->path().size() == size;
}

TEST_F(RegexPrecompilationExtensionTest, OptimizeableExpression) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches(r'[a-zA-Z]+[0-9]*')"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  EXPECT_THAT(plan, ExpressionPlanSizeIs(2));
}

TEST_F(RegexPrecompilationExtensionTest, DoesNotOptimizeParsedExpr) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr expr,
                       Parse("input.matches(r'[a-zA-Z]+[0-9]*')"));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CelExpression> plan,
      builder_.CreateExpression(&expr.expr(), &expr.source_info()));

  EXPECT_THAT(plan, ExpressionPlanSizeIs(3));
}

TEST_F(RegexPrecompilationExtensionTest, DoesNotOptimizeNonConstRegex) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches(input_re)"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  EXPECT_THAT(plan, ExpressionPlanSizeIs(3));
}

TEST_F(RegexPrecompilationExtensionTest, DoesNotOptimizeCompoundExpr) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches('abc' + 'def')"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  EXPECT_THAT(plan, ExpressionPlanSizeIs(5)) << expr.DebugString();
}

class RegexConstFoldInteropTest : public RegexPrecompilationExtensionTest {
 public:
  RegexConstFoldInteropTest() : RegexPrecompilationExtensionTest() {
    // TODO(uncreated-issue/27): This applies to either version of const folding.
    // Update when default is changed to new version.
    builder_.flat_expr_builder().set_constant_folding(true, &arena_);
  }

 protected:
  google::protobuf::Arena arena_;
};

TEST_F(RegexConstFoldInteropTest, StringConstantOptimizeable) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches('abc' + 'def')"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  EXPECT_THAT(plan, ExpressionPlanSizeIs(2)) << expr.DebugString();
}

TEST_F(RegexConstFoldInteropTest, WrongTypeNotOptimized) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches(123 + 456)"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  EXPECT_THAT(plan, ExpressionPlanSizeIs(3)) << expr.DebugString();
}

}  // namespace
}  // namespace google::api::expr::runtime
