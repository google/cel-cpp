#include "eval/eval/create_list_step.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/unknown_attribute_set.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using testing::Eq;
using testing::Not;

using google::api::expr::v1alpha1::Expr;

// Helper method. Creates simple pipeline containing Select step and runs it.
absl::StatusOr<CelValue> RunExpression(const std::vector<int64_t>& values,
                                       google::protobuf::Arena* arena,
                                       bool enable_unknowns) {
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

    path.push_back(std::move(const_step_status.value()));
  }

  auto step0_status = CreateCreateListStep(create_list, dummy_expr.id());

  if (!step0_status.ok()) {
    return step0_status.status();
  }

  path.push_back(std::move(step0_status.value()));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {},
                                 enable_unknowns);
  Activation activation;

  return cel_expr.Evaluate(activation, arena);
}

// Helper method. Creates simple pipeline containing Select step and runs it.
absl::StatusOr<CelValue> RunExpressionWithCelValues(
    const std::vector<CelValue>& values, google::protobuf::Arena* arena,
    bool enable_unknowns) {
  ExecutionPath path;
  Expr dummy_expr;

  Activation activation;
  auto create_list = dummy_expr.mutable_list_expr();
  int ind = 0;
  for (auto value : values) {
    std::string var_name = absl::StrCat("name_", ind++);
    auto expr0 = create_list->add_elements();
    expr0->set_id(ind);
    expr0->mutable_ident_expr()->set_name(var_name);

    auto ident_step_status = CreateIdentStep(&expr0->ident_expr(), expr0->id());
    if (!ident_step_status.ok()) {
      return ident_step_status.status();
    }

    path.push_back(std::move(ident_step_status.value()));
    activation.InsertValue(var_name, value);
  }

  auto step0_status = CreateCreateListStep(create_list, dummy_expr.id());

  if (!step0_status.ok()) {
    return step0_status.status();
  }

  path.push_back(std::move(step0_status.value()));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {},
                                 enable_unknowns);

  return cel_expr.Evaluate(activation, arena);
}

class CreateListStepTest : public testing::TestWithParam<bool> {};

// Tests error when not enough list elements are on the stack during list
// creation.
TEST(CreateListStepTest, TestCreateListStackUnderflow) {
  ExecutionPath path;
  Expr dummy_expr;

  auto create_list = dummy_expr.mutable_list_expr();
  auto expr0 = create_list->add_elements();
  expr0->mutable_const_expr()->set_int64_value(1);

  auto step0_status = CreateCreateListStep(create_list, dummy_expr.id());

  ASSERT_OK(step0_status);

  path.push_back(std::move(step0_status.value()));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {});
  Activation activation;

  google::protobuf::Arena arena;

  auto status = cel_expr.Evaluate(activation, &arena);
  ASSERT_FALSE(status.ok());
}

TEST_P(CreateListStepTest, CreateListEmpty) {
  google::protobuf::Arena arena;
  auto eval_result = RunExpression({}, &arena, GetParam());

  ASSERT_OK(eval_result);
  const CelValue result_value = eval_result.value();
  ASSERT_TRUE(result_value.IsList());
  EXPECT_THAT(result_value.ListOrDie()->size(), Eq(0));
}

TEST_P(CreateListStepTest, CreateListOne) {
  google::protobuf::Arena arena;
  auto eval_result = RunExpression({100}, &arena, GetParam());

  ASSERT_OK(eval_result);
  const CelValue result_value = eval_result.value();
  ASSERT_TRUE(result_value.IsList());
  EXPECT_THAT(result_value.ListOrDie()->size(), Eq(1));
  EXPECT_THAT((*result_value.ListOrDie())[0].Int64OrDie(), Eq(100));
}

TEST_P(CreateListStepTest, CreateListWithError) {
  google::protobuf::Arena arena;
  std::vector<CelValue> values;
  CelError error = absl::InvalidArgumentError("bad arg");
  values.push_back(CelValue::CreateError(&error));
  auto eval_result = RunExpressionWithCelValues(values, &arena, GetParam());

  ASSERT_OK(eval_result);
  const CelValue result_value = eval_result.value();
  ASSERT_TRUE(result_value.IsError());
  EXPECT_THAT(*result_value.ErrorOrDie(),
              Eq(absl::InvalidArgumentError("bad arg")));
}

TEST_P(CreateListStepTest, CreateListWithErrorAndUnknown) {
  google::protobuf::Arena arena;
  // list composition is: {unknown, error}
  std::vector<CelValue> values;
  Expr expr0;
  expr0.mutable_ident_expr()->set_name("name0");
  CelAttribute attr0(expr0, {});
  UnknownSet unknown_set0(UnknownAttributeSet({&attr0}));
  values.push_back(CelValue::CreateUnknownSet(&unknown_set0));
  CelError error = absl::InvalidArgumentError("bad arg");
  values.push_back(CelValue::CreateError(&error));

  auto eval_result = RunExpressionWithCelValues(values, &arena, GetParam());

  // The bad arg should win.
  ASSERT_OK(eval_result);
  const CelValue result_value = eval_result.value();
  ASSERT_TRUE(result_value.IsError());
  EXPECT_THAT(*result_value.ErrorOrDie(),
              Eq(absl::InvalidArgumentError("bad arg")));
}

TEST_P(CreateListStepTest, CreateListHundred) {
  google::protobuf::Arena arena;
  std::vector<int64_t> values;
  for (size_t i = 0; i < 100; i++) {
    values.push_back(i);
  }
  auto eval_result = RunExpression(values, &arena, GetParam());

  ASSERT_OK(eval_result);
  const CelValue result_value = eval_result.value();
  ASSERT_TRUE(result_value.IsList());
  EXPECT_THAT(result_value.ListOrDie()->size(),
              Eq(static_cast<int>(values.size())));
  for (size_t i = 0; i < values.size(); i++) {
    EXPECT_THAT((*result_value.ListOrDie())[i].Int64OrDie(), Eq(values[i]));
  }
}

TEST(CreateListStepTest, CreateListHundredAnd2Unknowns) {
  google::protobuf::Arena arena;
  std::vector<CelValue> values;

  Expr expr0;
  expr0.mutable_ident_expr()->set_name("name0");
  CelAttribute attr0(expr0, {});
  Expr expr1;
  expr1.mutable_ident_expr()->set_name("name1");
  CelAttribute attr1(expr1, {});
  UnknownSet unknown_set0(UnknownAttributeSet({&attr0}));
  UnknownSet unknown_set1(UnknownAttributeSet({&attr1}));
  for (size_t i = 0; i < 100; i++) {
    values.push_back(CelValue::CreateInt64(i));
  }
  values.push_back(CelValue::CreateUnknownSet(&unknown_set0));
  values.push_back(CelValue::CreateUnknownSet(&unknown_set1));

  auto eval_result = RunExpressionWithCelValues(values, &arena, true);

  ASSERT_OK(eval_result);
  const CelValue result_value = eval_result.value();
  ASSERT_TRUE(result_value.IsUnknownSet());
  const UnknownSet* result_set = result_value.UnknownSetOrDie();
  EXPECT_THAT(result_set->unknown_attributes().attributes().size(), Eq(2));
}

INSTANTIATE_TEST_SUITE_P(CombinedCreateListTest, CreateListStepTest,
                         testing::Bool());

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
