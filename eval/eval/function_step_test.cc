#include "eval/eval/function_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_function_adapter.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

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

class ConstFunction : public CelFunction {
 public:
  explicit ConstFunction(const CelValue& value, CelValue::Type type_kind)
      : CelFunction(CreateDescriptor(type_kind)), value_(value) {}

  static CelFunction::Descriptor CreateDescriptor(CelValue::Type kind) {
    return Descriptor{"", false, {}};
  }

  cel_base::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (!args.empty()) {
      return cel_base::Status(cel_base::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = value_;
    return cel_base::OkStatus();
  }

 private:
  CelValue value_;
};

class AddFunction : public CelFunction {
 public:
  AddFunction() : CelFunction(CreateDescriptor()) {}

  static CelFunction::Descriptor CreateDescriptor() {
    return Descriptor{
        "_+_", false, {CelValue::Type::kInt64, CelValue::Type::kInt64}};
  }

  cel_base::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 2 || !args[0].IsInt64() || !args[1].IsInt64()) {
      return cel_base::Status(cel_base::StatusCode::kInvalidArgument,
                          "Mismatched arguments passed to method");
    }

    int64_t arg0 = args[0].Int64OrDie();
    int64_t arg1 = args[1].Int64OrDie();

    *result = CelValue::CreateInt64(arg0 + arg1);
    return cel_base::OkStatus();
  }
};

TEST(FunctionStepTest, SimpleFunctionTest) {
  ExecutionPath path;

  ConstFunction const_func0(CelValue::CreateInt64(3), CelValue::Type::kInt64);
  ConstFunction const_func1(CelValue::CreateInt64(2), CelValue::Type::kInt64);

  AddFunction add_func;

  Expr dummy_expr0;
  Expr dummy_expr1;
  Expr dummy_expr2;

  auto step0_status = CreateFunctionStep(dummy_expr0.id(), {&const_func0});
  auto step1_status = CreateFunctionStep(dummy_expr1.id(), {&const_func1});
  auto step2_status = CreateFunctionStep(dummy_expr2.id(), {&add_func});

  ASSERT_TRUE(step0_status.ok());
  ASSERT_TRUE(step1_status.ok());
  ASSERT_TRUE(step2_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));
  path.push_back(std::move(step1_status.ValueOrDie()));
  path.push_back(std::move(step2_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  EXPECT_TRUE(status.ok());

  auto value = status.ValueOrDie();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(5));
}

TEST(FunctionStepTest, TestStackUnderflow) {
  ExecutionPath path;

  ConstFunction const_func0(CelValue::CreateInt64(3), CelValue::Type::kInt64);

  AddFunction add_func;

  Expr dummy_expr0;
  Expr dummy_expr2;

  auto step0_status = CreateFunctionStep(dummy_expr0.id(), {&const_func0});
  auto step2_status = CreateFunctionStep(dummy_expr2.id(), {&add_func});

  ASSERT_TRUE(step0_status.ok());
  ASSERT_TRUE(step2_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));
  path.push_back(std::move(step2_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  EXPECT_FALSE(status.ok());
}

// Test factory method when empty overload set is provided.
TEST(FunctionStepTest, TestNoOverloadsOnCreation) {
  Expr dummy_expr0;

  // function step with empty overloads
  auto step0_status = CreateFunctionStep(dummy_expr0.id(), {});

  EXPECT_FALSE(step0_status.ok());
}

TEST(FunctionStepTest, TestMultipleOverloads) {
  ExecutionPath path;

  ConstFunction const_func0(CelValue::CreateInt64(3), CelValue::Type::kInt64);

  Expr dummy_expr0;

  // function step with 2 identical overloads
  auto step0_status =
      CreateFunctionStep(dummy_expr0.id(), {&const_func0, &const_func0});

  ASSERT_TRUE(step0_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  EXPECT_FALSE(status.ok());
}

// Test situation when overloads match input arguments during evaliation.
TEST(FunctionStepTest, TestNoMatchingOverloadsDuringEvaluation) {
  ExecutionPath path;

  // Constants have UINT type, while AddFunction expects INT.
  ConstFunction const_func0(CelValue::CreateUint64(3), CelValue::Type::kUint64);
  ConstFunction const_func1(CelValue::CreateUint64(2), CelValue::Type::kUint64);

  AddFunction add_func;

  Expr dummy_expr0;
  Expr dummy_expr1;
  Expr dummy_expr2;

  auto step0_status = CreateFunctionStep(dummy_expr0.id(), {&const_func0});
  auto step1_status = CreateFunctionStep(dummy_expr1.id(), {&const_func1});
  auto step2_status = CreateFunctionStep(dummy_expr2.id(), {&add_func});

  ASSERT_TRUE(step0_status.ok());
  ASSERT_TRUE(step1_status.ok());
  ASSERT_TRUE(step2_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));
  path.push_back(std::move(step1_status.ValueOrDie()));
  path.push_back(std::move(step2_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  ASSERT_TRUE(status.ok());

  auto value = status.ValueOrDie();
  ASSERT_TRUE(value.IsError());
}


// Test situation when no overloads match input arguments during evaluation
// and at least one of arguments is error.
TEST(FunctionStepTest, TestNoMatchingOverloadsDuringEvaluationErrorForwarding) {
  ExecutionPath path;

  CelError error0;
  CelError error1;

  // Constants have ERROR type, while AddFunction expects INT.
  ConstFunction const_func0(CelValue::CreateError(&error0),
                            CelValue::Type::kError);
  ConstFunction const_func1(CelValue::CreateError(&error1),
                            CelValue::Type::kError);

  AddFunction add_func;

  Expr dummy_expr0;
  Expr dummy_expr1;
  Expr dummy_expr2;

  auto step0_status = CreateFunctionStep(dummy_expr0.id(), {&const_func0});
  auto step1_status = CreateFunctionStep(dummy_expr1.id(), {&const_func1});
  auto step2_status = CreateFunctionStep(dummy_expr2.id(), {&add_func});

  ASSERT_TRUE(step0_status.ok());
  ASSERT_TRUE(step1_status.ok());
  ASSERT_TRUE(step2_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));
  path.push_back(std::move(step1_status.ValueOrDie()));
  path.push_back(std::move(step2_status.ValueOrDie()));

  auto dummy_expr = absl::make_unique<google::api::expr::v1alpha1::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path), 0);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  ASSERT_TRUE(status.ok());

  auto value = status.ValueOrDie();

  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(value.ErrorOrDie(), Eq(&error0));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
