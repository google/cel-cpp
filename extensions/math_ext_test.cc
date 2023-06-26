// Copyright 2022 Google LLC
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

#include "extensions/math_ext.h"

#include <memory>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::SourceInfo;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ContainerBackedListImpl;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::InterpreterOptions;

using ::google::api::expr::runtime::test::EqualsCelValue;
using testing::HasSubstr;
using cel::internal::StatusIs;

constexpr absl::string_view kMathMin = "math.@min";
constexpr absl::string_view kMathMax = "math.@max";

struct TestCase {
  absl::string_view operation;
  CelValue arg1;
  absl::optional<CelValue> arg2;
  CelValue result;
};

TestCase MinCase(CelValue v1, CelValue v2, CelValue result) {
  return TestCase{kMathMin, v1, v2, result};
}

TestCase MinCase(CelValue list, CelValue result) {
  return TestCase{kMathMin, list, absl::nullopt, result};
}

TestCase MaxCase(CelValue v1, CelValue v2, CelValue result) {
  return TestCase{kMathMax, v1, v2, result};
}

TestCase MaxCase(CelValue list, CelValue result) {
  return TestCase{kMathMax, list, absl::nullopt, result};
}

Expr CallExprOneArg(absl::string_view operation) {
  Expr expr;
  auto call = expr.mutable_call_expr();
  call->set_function(operation);

  auto arg = call->add_args();
  auto ident = arg->mutable_ident_expr();
  ident->set_name("a");
  return expr;
}

Expr CallExprTwoArgs(absl::string_view operation) {
  Expr expr;
  auto call = expr.mutable_call_expr();
  call->set_function(operation);

  auto arg = call->add_args();
  auto ident = arg->mutable_ident_expr();
  ident->set_name("a");

  arg = call->add_args();
  ident = arg->mutable_ident_expr();
  ident->set_name("b");
  return expr;
}

void ExpectResult(const TestCase& test_case) {
  Expr expr;
  Activation activation;
  activation.InsertValue("a", test_case.arg1);
  if (test_case.arg2.has_value()) {
    activation.InsertValue("b", *test_case.arg2);
    expr = CallExprTwoArgs(test_case.operation);
  } else {
    expr = CallExprOneArg(test_case.operation);
  }

  SourceInfo source_info;
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterMathExtensionFunctions(builder->GetRegistry(), options));
  ASSERT_OK_AND_ASSIGN(auto cel_expression,
                       builder->CreateExpression(&expr, &source_info));

  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(auto value,
                       cel_expression->Evaluate(activation, &arena));
  if (!test_case.result.IsError()) {
    EXPECT_THAT(value, EqualsCelValue(test_case.result));
  } else {
    auto expected = test_case.result.ErrorOrDie();
    EXPECT_THAT(*value.ErrorOrDie(),
                StatusIs(expected->code(), HasSubstr(expected->message())));
  }
}

using MathExtParamsTest = testing::TestWithParam<TestCase>;
TEST_P(MathExtParamsTest, MinMaxTests) { ExpectResult(GetParam()); }

INSTANTIATE_TEST_SUITE_P(
    MathExtParamsTest, MathExtParamsTest,
    testing::ValuesIn<TestCase>({
        MinCase(CelValue::CreateInt64(3L), CelValue::CreateInt64(2L),
                CelValue::CreateInt64(2L)),
        MinCase(CelValue::CreateInt64(-1L), CelValue::CreateUint64(2u),
                CelValue::CreateInt64(-1L)),
        MinCase(CelValue::CreateInt64(-1L), CelValue::CreateDouble(-1.1),
                CelValue::CreateDouble(-1.1)),
        MinCase(CelValue::CreateDouble(-2.0), CelValue::CreateDouble(-1.1),
                CelValue::CreateDouble(-2.0)),
        MinCase(CelValue::CreateDouble(3.1), CelValue::CreateInt64(2),
                CelValue::CreateInt64(2)),
        MinCase(CelValue::CreateDouble(2.5), CelValue::CreateUint64(2u),
                CelValue::CreateUint64(2u)),
        MinCase(CelValue::CreateUint64(2u), CelValue::CreateDouble(-1.1),
                CelValue::CreateDouble(-1.1)),
        MinCase(CelValue::CreateUint64(3u), CelValue::CreateInt64(20),
                CelValue::CreateUint64(3u)),
        MinCase(CelValue::CreateUint64(4u), CelValue::CreateUint64(2u),
                CelValue::CreateUint64(2u)),
        MinCase(CelValue::CreateInt64(2L), CelValue::CreateUint64(2u),
                CelValue::CreateInt64(2L)),
        MinCase(CelValue::CreateInt64(-1L), CelValue::CreateDouble(-1.0),
                CelValue::CreateInt64(-1L)),
        MinCase(CelValue::CreateDouble(2.0), CelValue::CreateInt64(2),
                CelValue::CreateDouble(2.0)),
        MinCase(CelValue::CreateDouble(2.0), CelValue::CreateUint64(2u),
                CelValue::CreateDouble(2.0)),
        MinCase(CelValue::CreateUint64(2u), CelValue::CreateDouble(2.0),
                CelValue::CreateUint64(2u)),
        MinCase(CelValue::CreateUint64(3u), CelValue::CreateInt64(3),
                CelValue::CreateUint64(3u)),

        MaxCase(CelValue::CreateInt64(3L), CelValue::CreateInt64(2L),
                CelValue::CreateInt64(3L)),
        MaxCase(CelValue::CreateInt64(-1L), CelValue::CreateUint64(2u),
                CelValue::CreateUint64(2u)),
        MaxCase(CelValue::CreateInt64(-1L), CelValue::CreateDouble(-1.1),
                CelValue::CreateInt64(-1L)),
        MaxCase(CelValue::CreateDouble(-2.0), CelValue::CreateDouble(-1.1),
                CelValue::CreateDouble(-1.1)),
        MaxCase(CelValue::CreateDouble(3.1), CelValue::CreateInt64(2),
                CelValue::CreateDouble(3.1)),
        MaxCase(CelValue::CreateDouble(2.5), CelValue::CreateUint64(2u),
                CelValue::CreateDouble(2.5)),
        MaxCase(CelValue::CreateUint64(2u), CelValue::CreateDouble(-1.1),
                CelValue::CreateUint64(2u)),
        MaxCase(CelValue::CreateUint64(3u), CelValue::CreateInt64(20),
                CelValue::CreateInt64(20)),
        MaxCase(CelValue::CreateUint64(4u), CelValue::CreateUint64(2u),
                CelValue::CreateUint64(4u)),
        MaxCase(CelValue::CreateInt64(2L), CelValue::CreateUint64(2u),
                CelValue::CreateInt64(2L)),
        MaxCase(CelValue::CreateInt64(-1L), CelValue::CreateDouble(-1.0),
                CelValue::CreateInt64(-1L)),
        MaxCase(CelValue::CreateDouble(2.0), CelValue::CreateInt64(2),
                CelValue::CreateDouble(2.0)),
        MaxCase(CelValue::CreateDouble(2.0), CelValue::CreateUint64(2u),
                CelValue::CreateDouble(2.0)),
        MaxCase(CelValue::CreateUint64(2u), CelValue::CreateDouble(2.0),
                CelValue::CreateUint64(2u)),
        MaxCase(CelValue::CreateUint64(3u), CelValue::CreateInt64(3),
                CelValue::CreateUint64(3u)),
    }));

TEST(MathExtTest, MinMaxList) {
  ContainerBackedListImpl single_item_list({CelValue::CreateInt64(1)});
  ExpectResult(MinCase(CelValue::CreateList(&single_item_list),
                       CelValue::CreateInt64(1)));
  ExpectResult(MaxCase(CelValue::CreateList(&single_item_list),
                       CelValue::CreateInt64(1)));

  ContainerBackedListImpl list({CelValue::CreateInt64(1),
                                CelValue::CreateUint64(2u),
                                CelValue::CreateDouble(-1.1)});
  ExpectResult(
      MinCase(CelValue::CreateList(&list), CelValue::CreateDouble(-1.1)));
  ExpectResult(
      MaxCase(CelValue::CreateList(&list), CelValue::CreateUint64(2u)));

  absl::Status empty_list_err =
      absl::InvalidArgumentError("argument must not be empty");
  CelValue err_value = CelValue::CreateError(&empty_list_err);
  ContainerBackedListImpl empty_list({});
  ExpectResult(MinCase(CelValue::CreateList(&empty_list), err_value));
  ExpectResult(MaxCase(CelValue::CreateList(&empty_list), err_value));

  absl::Status bad_arg_err =
      absl::InvalidArgumentError("arguments must be numeric");
  err_value = CelValue::CreateError(&bad_arg_err);

  ContainerBackedListImpl bad_single_item({CelValue::CreateBool(true)});
  ExpectResult(MinCase(CelValue::CreateList(&bad_single_item), err_value));
  ExpectResult(MaxCase(CelValue::CreateList(&bad_single_item), err_value));

  ContainerBackedListImpl bad_middle_item({CelValue::CreateInt64(1),
                                           CelValue::CreateBool(false),
                                           CelValue::CreateDouble(-1.1)});
  ExpectResult(MinCase(CelValue::CreateList(&bad_middle_item), err_value));
  ExpectResult(MaxCase(CelValue::CreateList(&bad_middle_item), err_value));
}

}  // namespace
}  // namespace cel::extensions
