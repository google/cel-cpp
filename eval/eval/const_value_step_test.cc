#include "eval/eval/const_value_step.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/eval/evaluator_core.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using testing::Eq;

using google::api::expr::v1alpha1::Constant;
using google::api::expr::v1alpha1::Expr;

using google::protobuf::Arena;

cel_base::StatusOr<CelValue> RunConstantExpression(const Expr* expr,
                                               const Constant* const_expr,
                                               Arena* arena) {
  auto step_status =
      CreateConstValueStep(ConvertConstant(const_expr).value(), expr->id());
  if (!step_status.ok()) return step_status.status();

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  google::api::expr::v1alpha1::Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {});

  Activation activation;

  return impl.Evaluate(activation, arena);
}

TEST(ConstValueStepTest, TestEvaluationConstInt64) {
  Expr expr;
  auto const_expr = expr.mutable_const_expr();
  const_expr->set_int64_value(1);

  google::protobuf::Arena arena;

  auto status = RunConstantExpression(&expr, const_expr, &arena);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(1));
}

TEST(ConstValueStepTest, TestEvaluationConstUint64) {
  Expr expr;
  auto const_expr = expr.mutable_const_expr();
  const_expr->set_uint64_value(1);

  google::protobuf::Arena arena;

  auto status = RunConstantExpression(&expr, const_expr, &arena);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsUint64());
  EXPECT_THAT(value.Uint64OrDie(), Eq(1));
}

TEST(ConstValueStepTest, TestEvaluationConstBool) {
  Expr expr;
  auto const_expr = expr.mutable_const_expr();
  const_expr->set_bool_value(true);

  google::protobuf::Arena arena;

  auto status = RunConstantExpression(&expr, const_expr, &arena);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.BoolOrDie(), Eq(true));
}

TEST(ConstValueStepTest, TestEvaluationConstNull) {
  Expr expr;
  auto const_expr = expr.mutable_const_expr();
  const_expr->set_null_value(google::protobuf::NullValue(0));

  google::protobuf::Arena arena;

  auto status = RunConstantExpression(&expr, const_expr, &arena);

  ASSERT_OK(status);

  auto value = status.value();

  EXPECT_TRUE(value.IsNull());
}

TEST(ConstValueStepTest, TestEvaluationConstString) {
  Expr expr;
  auto const_expr = expr.mutable_const_expr();
  const_expr->set_string_value("test");

  google::protobuf::Arena arena;

  auto status = RunConstantExpression(&expr, const_expr, &arena);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsString());
  EXPECT_THAT(value.StringOrDie().value(), Eq("test"));
}

TEST(ConstValueStepTest, TestEvaluationConstDouble) {
  Expr expr;
  auto const_expr = expr.mutable_const_expr();
  const_expr->set_double_value(1.0);

  google::protobuf::Arena arena;

  auto status = RunConstantExpression(&expr, const_expr, &arena);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsDouble());
  EXPECT_THAT(value.DoubleOrDie(), testing::DoubleEq(1.0));
}

// Test Bytes constant
// For now, bytes are equivalent to string.
TEST(ConstValueStepTest, TestEvaluationConstBytes) {
  Expr expr;
  auto const_expr = expr.mutable_const_expr();
  const_expr->set_bytes_value("test");

  google::protobuf::Arena arena;

  auto status = RunConstantExpression(&expr, const_expr, &arena);

  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsBytes());
  EXPECT_THAT(value.BytesOrDie().value(), Eq("test"));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
