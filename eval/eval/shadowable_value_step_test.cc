#include "eval/eval/shadowable_value_step.h"

#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/descriptor.h"
#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/test_type_registry.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;
using testing::Eq;

absl::StatusOr<CelValue> RunShadowableExpression(const std::string& identifier,
                                                 const CelValue& value,
                                                 const Activation& activation,
                                                 Arena* arena) {
  CEL_ASSIGN_OR_RETURN(auto step,
                       CreateShadowableValueStep(identifier, value, 1));
  ExecutionPath path;
  path.push_back(std::move(step));

  cel::ast::internal::Expr dummy_expr;
  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), &TestTypeRegistry(),
                             0, {});
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

}  // namespace google::api::expr::runtime
