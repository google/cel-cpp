#include "eval/eval/logic_step.h"

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
  absl::Status EvaluateLogic(CelValue arg0, CelValue arg1, bool is_or,
                             CelValue* result, bool enable_unknown) {
    Expr expr0;
    auto ident_expr0 = expr0.mutable_ident_expr();
    ident_expr0->set_name("name0");

    Expr expr1;
    auto ident_expr1 = expr1.mutable_ident_expr();
    ident_expr1->set_name("name1");

    ExecutionPath path;

    auto step_status = CreateIdentStep(ident_expr0, expr0.id());
    if (!step_status.ok()) return step_status.status();

    path.push_back(std::move(step_status).value());

    step_status = CreateIdentStep(ident_expr1, expr1.id());
    if (!step_status.ok()) return step_status.status();

    path.push_back(std::move(step_status).value());

    step_status = (is_or) ? CreateOrStep(2) : CreateAndStep(2);
    if (!step_status.ok()) return step_status.status();

    path.push_back(std::move(step_status).value());

    auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

    CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0, {},
                               enable_unknown);

    Activation activation;
    std::string value("test");

    activation.InsertValue("name0", arg0);
    activation.InsertValue("name1", arg1);
    auto status0 = impl.Evaluate(activation, &arena_);
    if (!status0.ok()) return status0.status();

    *result = status0.value();
    return absl::OkStatus();
  }

 private:
  Arena arena_;
};

TEST_P(LogicStepTest, TestAndLogic) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(false),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(true),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(false),
                    false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestOrLogic) {
  CelValue result;
  absl::Status status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(true),
                    true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(true), CelValue::CreateBool(false),
                    true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(CelValue::CreateBool(false),
                         CelValue::CreateBool(true), true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status =
      EvaluateLogic(CelValue::CreateBool(false), CelValue::CreateBool(false),
                    true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestAndLogicErrorHandling) {
  CelValue result;
  CelError error;
  CelValue error_value = CelValue::CreateError(&error);
  absl::Status status = EvaluateLogic(error_value, CelValue::CreateBool(true),
                                      false, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(true), error_value, false,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(false), error_value, false,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(error_value, CelValue::CreateBool(false), false,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());
}

TEST_P(LogicStepTest, TestOrLogicErrorHandling) {
  CelValue result;
  CelError error;
  CelValue error_value = CelValue::CreateError(&error);
  absl::Status status = EvaluateLogic(error_value, CelValue::CreateBool(false),
                                      true, &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(false), error_value, true,
                         &result, GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsError());

  status = EvaluateLogic(CelValue::CreateBool(true), error_value, true, &result,
                         GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(error_value, CelValue::CreateBool(true), true, &result,
                         GetParam());
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());
}

TEST_F(LogicStepTest, TestAndLogicUnknownHandling) {
  CelValue result;
  UnknownSet unknown_set;
  CelError cel_error;
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);
  CelValue error_value = CelValue::CreateError(&cel_error);
  absl::Status status = EvaluateLogic(unknown_value, CelValue::CreateBool(true),
                                      false, &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(true), unknown_value, false,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(false), unknown_value, false,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, CelValue::CreateBool(false), false,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_FALSE(result.BoolOrDie());

  status = EvaluateLogic(error_value, unknown_value, false, &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(unknown_value, error_value, false, &result, true);
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
                         CelValue::CreateUnknownSet(&unknown_set1), false,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());
  ASSERT_THAT(
      result.UnknownSetOrDie()->unknown_attributes().attributes().size(),
      Eq(2));
}

TEST_F(LogicStepTest, TestOrLogicUnknownHandling) {
  CelValue result;
  UnknownSet unknown_set;
  CelError cel_error;
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);
  CelValue error_value = CelValue::CreateError(&cel_error);
  absl::Status status = EvaluateLogic(
      unknown_value, CelValue::CreateBool(false), true, &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(false), unknown_value, true,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(CelValue::CreateBool(true), unknown_value, true,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, CelValue::CreateBool(true), true,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsBool());
  ASSERT_TRUE(result.BoolOrDie());

  status = EvaluateLogic(unknown_value, error_value, true, &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());

  status = EvaluateLogic(error_value, unknown_value, true, &result, true);
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
                         CelValue::CreateUnknownSet(&unknown_set1), true,
                         &result, true);
  ASSERT_OK(status);
  ASSERT_TRUE(result.IsUnknownSet());
  ASSERT_THAT(
      result.UnknownSetOrDie()->unknown_attributes().attributes().size(),
      Eq(2));
}

INSTANTIATE_TEST_SUITE_P(LogicStepTest, LogicStepTest, testing::Bool());
}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
