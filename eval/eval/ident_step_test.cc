#include "eval/eval/ident_step.h"
#include "eval/eval/evaluator_core.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::api::expr::v1alpha1::Expr;
using google::protobuf::FieldMask;
using testing::Eq;

using google::protobuf::Arena;

TEST(IdentStepTest, TestIdentStep) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_TRUE(step_status.ok());

  ExecutionPath path;
  path.push_back(std::move(step_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_TRUE(status0.ok());

  CelValue result = status0.ValueOrDie();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));
}

TEST(IdentStepTest, TestIdentStepNameNotFound) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_TRUE(step_status.ok());

  ExecutionPath path;
  path.push_back(std::move(step_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  Arena arena;
  std::string value("test");

  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_TRUE(status0.ok());

  CelValue result = status0.ValueOrDie();
  ASSERT_TRUE(result.IsError());
}

TEST(IdentStepTest, TestIdentStepUnknownValue) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_TRUE(step_status.ok());

  ExecutionPath path;
  path.push_back(std::move(step_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_TRUE(status0.ok());

  CelValue result = status0.ValueOrDie();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  FieldMask unknown_mask;
  unknown_mask.add_paths("name0");

  activation.set_unknown_paths(unknown_mask);
  status0 = impl.Evaluate(activation, &arena);
  ASSERT_TRUE(status0.ok());

  result = status0.ValueOrDie();

  ASSERT_TRUE(result.IsError());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
