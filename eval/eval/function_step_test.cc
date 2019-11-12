#include "eval/eval/function_step.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using testing::Eq;
using testing::Not;

using google::api::expr::v1alpha1::Expr;

int GetExprId() {
  static int id = 0;
  id++;
  return id;
}

class ConstFunction : public CelFunction {
 public:
  explicit ConstFunction(const CelValue& value, absl::string_view name)
      : CelFunction(CreateDescriptor(name)), value_(value) {}

  static CelFunction::Descriptor CreateDescriptor(absl::string_view name) {
    return Descriptor{std::string(name), false, {}};
  }

  static Expr::Call MakeCall(absl::string_view name) {
    Expr::Call call;
    call.set_function(name.data());
    call.clear_target();
    return call;
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

  static Expr::Call MakeCall() {
    Expr::Call call;
    call.set_function("_+_");
    call.add_args();
    call.add_args();
    call.clear_target();
    return call;
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

// Create and initialize a registry with some default functions.
void AddDefaults(CelFunctionRegistry& registry) {
  EXPECT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateInt64(3), "Const3"))
                  .ok());
  EXPECT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateInt64(2), "Const2"))
                  .ok());
  EXPECT_TRUE(registry.Register(absl::make_unique<AddFunction>()).ok());
}

TEST(FunctionStepTest, SimpleFunctionTest) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call call2 = ConstFunction::MakeCall("Const2");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status = CreateFunctionStep(&call1, GetExprId(), registry);
  auto step1_status = CreateFunctionStep(&call2, GetExprId(), registry);
  auto step2_status = CreateFunctionStep(&add_call, GetExprId(), registry);

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

  CelFunctionRegistry registry;
  AddDefaults(registry);

  AddFunction add_func;

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status = CreateFunctionStep(&call1, GetExprId(), registry);
  auto step2_status = CreateFunctionStep(&add_call, GetExprId(), registry);

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
  CelFunctionRegistry registry;  // SetupRegistry();
  Expr::Call call = ConstFunction::MakeCall("Const0");

  // function step with empty overloads
  auto step0_status = CreateFunctionStep(&call, GetExprId(), registry);

  EXPECT_FALSE(step0_status.ok());
}

// Test situation when no overloads match input arguments during evaluation.
TEST(FunctionStepTest, TestNoMatchingOverloadsDuringEvaluation) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  ASSERT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateUint64(4), "Const4"))
                  .ok());

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call call2 = ConstFunction::MakeCall("Const4");
  // Add expects {int64_t, int64_t} but it's {int64_t, uint64_t}.
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status = CreateFunctionStep(&call1, GetExprId(), registry);
  auto step1_status = CreateFunctionStep(&call2, GetExprId(), registry);
  auto step2_status = CreateFunctionStep(&add_call, GetExprId(), registry);

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

  CelFunctionRegistry registry;
  AddDefaults(registry);

  CelError error0;
  CelError error1;

  // Constants have ERROR type, while AddFunction expects INT.
  ASSERT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateError(&error0), "ConstError1"))
                  .ok());
  ASSERT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateError(&error1), "ConstError2"))
                  .ok());

  Expr::Call call1 = ConstFunction::MakeCall("ConstError1");
  Expr::Call call2 = ConstFunction::MakeCall("ConstError2");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status = CreateFunctionStep(&call1, GetExprId(), registry);
  auto step1_status = CreateFunctionStep(&call2, GetExprId(), registry);
  auto step2_status = CreateFunctionStep(&add_call, GetExprId(), registry);

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
