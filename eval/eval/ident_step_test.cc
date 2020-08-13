#include "eval/eval/ident_step.h"

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

using google::api::expr::v1alpha1::Expr;
using google::protobuf::FieldMask;
using testing::Eq;

using google::protobuf::Arena;

TEST(IdentStepTest, TestIdentStep) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_OK(step_status);

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {});

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));
}

TEST(IdentStepTest, TestIdentStepNameNotFound) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_OK(step_status);

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {});

  Activation activation;
  Arena arena;
  std::string value("test");

  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();
  ASSERT_TRUE(result.IsError());
}

TEST(IdentStepTest, DisableMissingAttributeErrorsOK) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_OK(step_status);

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {},
                             /*enable_unknowns=*/false);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  const CelAttributePattern pattern("name0", {});
  activation.set_missing_attribute_patterns({pattern});

  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  EXPECT_THAT(status0.value().StringOrDie().value(), Eq("test"));
}

TEST(IdentStepTest, TestIdentStepMissingAttributeErrors) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_OK(step_status);

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {}, false,
                             false, /*enable_missing_attribute_errors=*/true);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  CelAttributePattern pattern("name0", {});
  activation.set_missing_attribute_patterns({pattern});

  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  EXPECT_EQ(status0.value().ErrorOrDie()->code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status0.value().ErrorOrDie()->message(),
            "MissingAttributeError: name0");
}

TEST(IdentStepTest, TestIdentStepUnknownValueError) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_OK(step_status);

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {});

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  FieldMask unknown_mask;
  unknown_mask.add_paths("name0");

  activation.set_unknown_paths(unknown_mask);
  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  result = status0.value();

  ASSERT_TRUE(result.IsError());
  ASSERT_TRUE(IsUnknownValueError(result));
  EXPECT_THAT(GetUnknownPathsSetOrDie(result),
              Eq(std::set<std::string>({"name0"})));
}

TEST(IdentStepTest, TestIdentStepUnknownAttribute) {
  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();
  ident_expr->set_name("name0");

  auto step_status = CreateIdentStep(ident_expr, expr.id());
  ASSERT_OK(step_status);

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  // Expression with unknowns enabled.
  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {}, true);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  std::vector<CelAttributePattern> unknown_patterns;
  unknown_patterns.push_back(CelAttributePattern("name_bad", {}));

  activation.set_unknown_attribute_patterns(unknown_patterns);
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  unknown_patterns.push_back(CelAttributePattern("name0", {}));

  activation.set_unknown_attribute_patterns(unknown_patterns);
  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  result = status0.value();

  ASSERT_TRUE(result.IsUnknownSet());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
