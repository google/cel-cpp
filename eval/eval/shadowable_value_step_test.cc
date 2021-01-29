#include "eval/eval/shadowable_value_step.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_value.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::protobuf::Arena;
using testing::Eq;

absl::StatusOr<CelValue> RunShadowableExpression(const std::string& identifier,
                                                 const CelValue& value,
                                                 const Activation& activation,
                                                 Arena* arena) {
  auto step_status = CreateShadowableValueStep(identifier, value, 1);
  if (!step_status.ok()) {
    return step_status.status();
  }

  ExecutionPath path;
  path.push_back(std::move(step_status.value()));

  google::api::expr::v1alpha1::Expr dummy_expr;
  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {});
  return impl.Evaluate(activation, arena);
}

TEST(ShadowableValueStepTest, TestEvaluateNoShadowing) {
  std::string type_name = "google.api.expr.runtime.TestMessage";

  Activation activation;
  Arena arena;

  auto type_value =
      CelValue::CreateCelType(CelValue::CelTypeHolder(&type_name));
  auto status =
      RunShadowableExpression(type_name, type_value, activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();
  ASSERT_TRUE(value.IsCelType());
  EXPECT_THAT(value.CelTypeOrDie().value(), Eq(type_name));
}

TEST(ShadowableValueStepTest, TestEvaluateShadowedIdentifier) {
  std::string type_name = "int";
  auto shadow_value = CelValue::CreateInt64(1024L);

  Activation activation;
  activation.InsertValue(type_name, shadow_value);
  Arena arena;

  auto type_value =
      CelValue::CreateCelType(CelValue::CelTypeHolder(&type_name));
  auto status =
      RunShadowableExpression(type_name, type_value, activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();
  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(1024L));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
