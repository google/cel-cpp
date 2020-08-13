#include "eval/eval/ternary_step.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/eval/ident_step.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::api::expr::v1alpha1::Expr;

using google::protobuf::Arena;
using testing::Eq;
class LogicStepTest : public testing::TestWithParam<bool> {
 public:
  absl::Status EvaluateLogic(CelValue arg0, CelValue arg1, CelValue arg2,
                             CelValue* result, bool enable_unknown) {
    Expr expr0;
    expr0.set_id(1);
    auto ident_expr0 = expr0.mutable_ident_expr();
    ident_expr0->set_name("name0");

    Expr expr1;
    expr1.set_id(2);
    auto ident_expr1 = expr1.mutable_ident_expr();
    ident_expr1->set_name("name1");

    Expr expr2;
    expr2.set_id(3);
    auto ident_expr2 = expr2.mutable_ident_expr();
    ident_expr2->set_name("name2");

    ExecutionPath path;

    auto step_status = CreateIdentStep(ident_expr0, expr0.id());
    if (!step_status.ok()) {
      return step_status.status();
    }

    path.push_back(std::move(step_status).value());

    step_status = CreateIdentStep(ident_expr1, expr1.id());
    if (!step_status.ok()) {
      return step_status.status();
    }

    path.push_back(std::move(step_status).value());

    step_status = CreateIdentStep(ident_expr2, expr2.id());
    if (!step_status.ok()) {
      return step_status.status();
    }

    path.push_back(std::move(step_status).value());

    step_status = CreateTernaryStep(4);
    if (!step_status.ok()) {
      return step_status.status();
    }

    path.push_back(std::move(step_status).value());

    auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

    CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {},
                               enable_unknown);

    Activation activation;
    std::string value("test");

    activation.InsertValue("name0", arg0);
    activation.InsertValue("name1", arg1);
    activation.InsertValue("name2", arg2);
    auto status0 = impl.Evaluate(activation, &arena_);
    if (!status0.ok()) return status0.status();

    *result = status0.value();
    return absl::OkStatus();
  }

 private:
  Arena arena_;
};

TEST_P(LogicStepTest, TestBoolCond) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    CelValue::CreateBool(false), &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(true),
                    CelValue::CreateBool(false), &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestErrorHandling) {
  CelValue result;
  CelError error;
  CelValue error_value = CelValue::CreateError(&error);
  absl::Status status =
      EvaluateLogic(error_value, CelValue::CreateBool(true),
                    CelValue::CreateBool(false), &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(true), error_value,
                         CelValue::CreateBool(false), &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(false), error_value,
                         CelValue::CreateBool(false), &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_F(LogicStepTest, TestUnknownHandling) {
  CelValue result;
  UnknownSet unknown_set;
  CelError cel_error;
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);
  CelValue error_value = CelValue::CreateError(&cel_error);
  absl::Status status =
      EvaluateLogic(unknown_value, CelValue::CreateBool(true),
                    CelValue::CreateBool(false), &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(true), unknown_value,
                         CelValue::CreateBool(false), &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(false), unknown_value,
                         CelValue::CreateBool(false), &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(error_value, unknown_value,
                         CelValue::CreateBool(false), &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(unknown_value, error_value,
                         CelValue::CreateBool(false), &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  Expr expr0;
  auto ident_expr0 = expr0.mutable_ident_expr();
  ident_expr0->set_name("name0");

  Expr expr1;
  auto ident_expr1 = expr1.mutable_ident_expr();
  ident_expr1->set_name("name1");

  CelAttribute attr0(expr0, {}), attr1(expr1, {});
  UnknownAttributeSet unknown_attr_set0({&attr0});
  UnknownAttributeSet unknown_attr_set1({&attr1});
  UnknownSet unknown_set0(unknown_attr_set0);
  UnknownSet unknown_set1(unknown_attr_set1);

  EXPECT_THAT(unknown_attr_set0.attributes().size(), Eq(1));
  EXPECT_THAT(unknown_attr_set1.attributes().size(), Eq(1));

  status = EvaluateLogic(CelValue::CreateUnknownSet(&unknown_set0),
                         CelValue::CreateUnknownSet(&unknown_set1),
                         CelValue::CreateBool(false), &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());
  const auto& attrs =
      result.UnknownSetOrDie()->unknown_attributes().attributes();
  ASSERT_THAT(attrs, testing::SizeIs(1));
  EXPECT_THAT(attrs[0]->variable().ident_expr().name(), Eq("name0"));
}

INSTANTIATE_TEST_SUITE_P(LogicStepTest, LogicStepTest, testing::Bool());
}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
