// Copyright 2023 Google LLC
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
//
// Smoke tests for CelExpressionBuilderFlatImpl. This class is a thin wrapper
// over FlatExprBuilder, so most of the tests are just covering the conversion
// code from the legacy APIs to the implementation. See
// flat_expr_builder_test.cc for additional tests.
#include "eval/compiler/cel_expression_builder_flat_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/runtime_options.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::v1alpha1::SourceInfo;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::test::v1::proto3::NestedTestAllTypes;
using testing::_;
using testing::Contains;
using testing::HasSubstr;
using testing::IsNull;
using testing::NotNull;
using cel::internal::StatusIs;

TEST(CelExpressionBuilderFlatImplTest, Error) {
  Expr expr;
  SourceInfo source_info;
  CelExpressionBuilderFlatImpl builder;
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid empty expression")));
}

TEST(CelExpressionBuilderFlatImplTest, ParsedExpr) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));

  CelExpressionBuilderFlatImpl builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelInt64(3));
}

struct RecursiveTestCase {
  std::string test_name;
  std::string expr;
  test::CelValueMatcher matcher;
};

class RecursivePlanTest : public ::testing::TestWithParam<RecursiveTestCase> {};

TEST_P(RecursivePlanTest, ParsedExprRecursiveOptimizedImpl) {
  const RecursiveTestCase& test_case = GetParam();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expr));
  cel::RuntimeOptions options;
  google::protobuf::Arena arena;
  // Unbounded.
  options.max_recursion_depth = -1;
  CelExpressionBuilderFlatImpl builder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  EXPECT_THAT(dynamic_cast<const CelExpressionRecursiveImpl*>(plan.get()),
              NotNull());

  Activation activation;
  activation.InsertValue("int_1", CelValue::CreateInt64(1));
  activation.InsertValue("string_abc", CelValue::CreateStringView("abc"));
  activation.InsertValue("string_def", CelValue::CreateStringView("def"));
  CelMapBuilder map;
  ASSERT_OK(map.Add(CelValue::CreateStringView("a"), CelValue::CreateInt64(1)));
  ASSERT_OK(map.Add(CelValue::CreateStringView("b"), CelValue::CreateInt64(2)));
  activation.InsertValue("map_var", CelValue::CreateMap(&map));
  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_payload()->set_single_int64(42);
  activation.InsertValue("struct_var",
                         CelProtoWrapper::CreateMessage(&msg, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test_case.matcher);
}

TEST_P(RecursivePlanTest, Disabled) {
  const RecursiveTestCase& test_case = GetParam();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expr));
  cel::RuntimeOptions options;
  google::protobuf::Arena arena;
  // disabled.
  options.max_recursion_depth = 0;
  CelExpressionBuilderFlatImpl builder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  EXPECT_THAT(dynamic_cast<const CelExpressionRecursiveImpl*>(plan.get()),
              IsNull());

  Activation activation;
  activation.InsertValue("int_1", CelValue::CreateInt64(1));
  activation.InsertValue("string_abc", CelValue::CreateStringView("abc"));
  activation.InsertValue("string_def", CelValue::CreateStringView("def"));
  CelMapBuilder map;
  ASSERT_OK(map.Add(CelValue::CreateStringView("a"), CelValue::CreateInt64(1)));
  ASSERT_OK(map.Add(CelValue::CreateStringView("b"), CelValue::CreateInt64(2)));
  activation.InsertValue("map_var", CelValue::CreateMap(&map));
  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_payload()->set_single_int64(42);
  activation.InsertValue("struct_var",
                         CelProtoWrapper::CreateMessage(&msg, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test_case.matcher);
}

INSTANTIATE_TEST_SUITE_P(
    RecursivePlanTest, RecursivePlanTest,
    testing::ValuesIn(std::vector<RecursiveTestCase>{
        {"constant", "'abc'", test::IsCelString("abc")},
        {"call", "1 + 2", test::IsCelInt64(3)},
        {"nested_call", "1 + 1 + 1 + 1", test::IsCelInt64(4)},
        {"and", "true && false", test::IsCelBool(false)},
        {"or", "true || false", test::IsCelBool(true)},
        {"ternary", "(true || false) ? 2 + 2 : 3 + 3", test::IsCelInt64(4)},
        {"create_list", "3 in [1, 2, 3]", test::IsCelBool(true)},
        {"create_list_complex", "3 in [2 / 2, 4 / 2, 6 / 2]",
         test::IsCelBool(true)},
        {"ident", "int_1 == 1", test::IsCelBool(true)},
        {"ident_complex", "int_1 + 2 > 4 ? string_abc : string_def",
         test::IsCelString("def")},
        {"select", "struct_var.child.payload.single_int64",
         test::IsCelInt64(42)},
        {"nested_select", "[map_var.a, map_var.b].size() == 2",
         test::IsCelBool(true)},
        {"map_index", "map_var['b']", test::IsCelInt64(2)},
        {"list_index", "[1, 2, 3][1]", test::IsCelInt64(2)},
        {"compre_exists", "[1, 2, 3, 4].exists(x, x == 3)",
         test::IsCelBool(true)},
        {"compre_map", "8 in [1, 2, 3, 4].map(x, x * 2)",
         test::IsCelBool(true)},
        {"map_var_compre_exists", "map_var.exists(key, key == 'b')",
         test::IsCelBool(true)},
        {"map_compre_exists", "{'a': 1, 'b': 2}.exists(k, k == 'b')",
         test::IsCelBool(true)},
        {"create_map", "{'a': 42, 'b': 0, 'c': 0}.size()", test::IsCelInt64(3)},
    }),

    [](const testing::TestParamInfo<RecursiveTestCase>& info) -> std::string {
      return info.param.test_name;
    });

TEST(CelExpressionBuilderFlatImplTest, ParsedExprWithWarnings) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));
  cel::RuntimeOptions options;
  options.fail_on_warnings = false;

  CelExpressionBuilderFlatImpl builder(options);
  std::vector<absl::Status> warnings;

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CelExpression> plan,
      builder.CreateExpression(&parsed_expr.expr(), &parsed_expr.source_info(),
                               &warnings));

  EXPECT_THAT(warnings, Contains(StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("No overloads"))));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelError(
                          StatusIs(_, HasSubstr("No matching overloads"))));
}

TEST(CelExpressionBuilderFlatImplTest, CheckedExpr) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));
  CheckedExpr checked_expr;
  checked_expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  checked_expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());

  CelExpressionBuilderFlatImpl builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&checked_expr));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelInt64(3));
}

TEST(CelExpressionBuilderFlatImplTest, CheckedExprWithWarnings) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));
  CheckedExpr checked_expr;
  checked_expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  checked_expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  cel::RuntimeOptions options;
  options.fail_on_warnings = false;

  CelExpressionBuilderFlatImpl builder(options);
  std::vector<absl::Status> warnings;

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&checked_expr, &warnings));

  EXPECT_THAT(warnings, Contains(StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("No overloads"))));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelError(
                          StatusIs(_, HasSubstr("No matching overloads"))));
}

}  // namespace

}  // namespace google::api::expr::runtime
