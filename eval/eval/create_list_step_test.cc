#include "eval/eval/create_list_step.h"
#include "eval/eval/const_value_step.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using testing::Eq;
using testing::Not;

using google::api::expr::v1alpha1::Expr;

// Helper method. Creates simple pipeline containing Select step and runs it.
cel_base::StatusOr<CelValue> RunExpression(const std::vector<int64_t>& values,
                                       google::protobuf::Arena* arena) {
  ExecutionPath path;
  Expr dummy_expr;

  auto create_list = dummy_expr.mutable_list_expr();
  for (auto value : values) {
    auto expr0 = create_list->add_elements();
    expr0->mutable_const_expr()->set_int64_value(value);
    auto const_step_status = CreateConstValueStep(
        ConvertConstant(&expr0->const_expr()).value(), expr0->id());
    if (!const_step_status.ok()) {
      return const_step_status.status();
    }

    path.push_back(std::move(const_step_status.ValueOrDie()));
  }

  auto step0_status = CreateCreateListStep(create_list, dummy_expr.id());

  if (!step0_status.ok()) {
    return step0_status.status();
  }

  path.push_back(std::move(step0_status.ValueOrDie()));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0);
  Activation activation;

  return cel_expr.Evaluate(activation, arena);
}

// Tests error when not enough list elements are on the stack during list
// creation.
TEST(CreateListStepTest, TestCreateListStackUndeflow) {
  ExecutionPath path;
  Expr dummy_expr;

  auto create_list = dummy_expr.mutable_list_expr();
  auto expr0 = create_list->add_elements();
  expr0->mutable_const_expr()->set_int64_value(1);

  auto step0_status = CreateCreateListStep(create_list, dummy_expr.id());

  ASSERT_TRUE(step0_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0);
  Activation activation;

  google::protobuf::Arena arena;

  auto status = cel_expr.Evaluate(activation, &arena);
  ASSERT_FALSE(status.ok());
}

TEST(CreateListStepTest, CreateListEmpty) {
  google::protobuf::Arena arena;
  auto eval_result = RunExpression({}, &arena);

  ASSERT_TRUE(eval_result.ok());
  const CelValue result_value = eval_result.ValueOrDie();
  ASSERT_TRUE(result_value.IsList());
  EXPECT_THAT(result_value.ListOrDie()->size(), Eq(0));
}

TEST(CreateListStepTest, CreateListOne) {
  google::protobuf::Arena arena;
  auto eval_result = RunExpression({100}, &arena);

  ASSERT_TRUE(eval_result.ok());
  const CelValue result_value = eval_result.ValueOrDie();
  ASSERT_TRUE(result_value.IsList());
  EXPECT_THAT(result_value.ListOrDie()->size(), Eq(1));
  EXPECT_THAT((*result_value.ListOrDie())[0].Int64OrDie(), Eq(100));
}

TEST(CreateListStepTest, CreateListHundred) {
  google::protobuf::Arena arena;
  std::vector<int64_t> values;
  for (int i = 0; i < 100; i++) {
    values.push_back(i);
  }
  auto eval_result = RunExpression(values, &arena);

  ASSERT_TRUE(eval_result.ok());
  const CelValue result_value = eval_result.ValueOrDie();
  ASSERT_TRUE(result_value.IsList());
  EXPECT_THAT(result_value.ListOrDie()->size(), Eq(values.size()));
  for (int i = 0; i < values.size(); i++) {
    EXPECT_THAT((*result_value.ListOrDie())[i].Int64OrDie(), Eq(values[i]));
  }
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
