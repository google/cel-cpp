#include "eval/eval/function_step.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/eval/ident_step.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_function_result_set.h"
#include "eval/testutil/test_message.pb.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::Not;
using testing::UnorderedElementsAre;

using google::api::expr::v1alpha1::Expr;

int GetExprId() {
  static int id = 0;
  id++;
  return id;
}

// Simple function that takes no arguments and returns a constant value.
class ConstFunction : public CelFunction {
 public:
  explicit ConstFunction(const CelValue& value, absl::string_view name)
      : CelFunction(CreateDescriptor(name)), value_(value) {}

  static CelFunctionDescriptor CreateDescriptor(absl::string_view name) {
    return CelFunctionDescriptor{std::string(name), false, {}};
  }

  static Expr::Call MakeCall(absl::string_view name) {
    Expr::Call call;
    call.set_function(name.data());
    call.clear_target();
    return call;
  }

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (!args.empty()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = value_;
    return absl::OkStatus();
  }

 private:
  CelValue value_;
};

enum class ShouldReturnUnknown : bool { kYes = true, kNo = false };

class AddFunction : public CelFunction {
 public:
  AddFunction()
      : CelFunction(CreateDescriptor()), should_return_unknown_(false) {}
  explicit AddFunction(ShouldReturnUnknown should_return_unknown)
      : CelFunction(CreateDescriptor()),
        should_return_unknown_(static_cast<bool>(should_return_unknown)) {}

  static CelFunctionDescriptor CreateDescriptor() {
    return CelFunctionDescriptor{
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

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 2 || !args[0].IsInt64() || !args[1].IsInt64()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Mismatched arguments passed to method");
    }
    if (should_return_unknown_) {
      *result =
          CreateUnknownFunctionResultError(arena, "Add can't be resolved.");
      return absl::OkStatus();
    }

    int64_t arg0 = args[0].Int64OrDie();
    int64_t arg1 = args[1].Int64OrDie();

    *result = CelValue::CreateInt64(arg0 + arg1);
    return absl::OkStatus();
  }

 private:
  bool should_return_unknown_;
};

class SinkFunction : public CelFunction {
 public:
  SinkFunction(CelValue::Type type) : CelFunction(CreateDescriptor(type)) {}

  static CelFunctionDescriptor CreateDescriptor(CelValue::Type type) {
    return CelFunctionDescriptor{"Sink", false, {type}};
  }

  static Expr::Call MakeCall() {
    Expr::Call call;
    call.set_function("Sink");
    call.add_args();
    call.clear_target();
    return call;
  }

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    // Return value is ignored.
    *result = CelValue::CreateInt64(0);
    return absl::OkStatus();
  }
};

// Create and initialize a registry with some default functions.
void AddDefaults(CelFunctionRegistry& registry) {
  static UnknownSet* unknown_set = new UnknownSet();
  EXPECT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateInt64(3), "Const3"))
                  .ok());
  EXPECT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateInt64(2), "Const2"))
                  .ok());
  EXPECT_TRUE(registry
                  .Register(absl::make_unique<ConstFunction>(
                      CelValue::CreateUnknownSet(unknown_set), "ConstUnknown"))
                  .ok());
  EXPECT_TRUE(registry.Register(absl::make_unique<AddFunction>()).ok());

  EXPECT_TRUE(
      registry.Register(absl::make_unique<SinkFunction>(CelValue::Type::kList))
          .ok());

  EXPECT_TRUE(
      registry.Register(absl::make_unique<SinkFunction>(CelValue::Type::kMap))
          .ok());

  EXPECT_TRUE(
      registry
          .Register(absl::make_unique<SinkFunction>(CelValue::Type::kMessage))
          .ok());
}

// Test common functions with varying levels of unknown support.
class FunctionStepTest
    : public testing::TestWithParam<UnknownProcessingOptions> {
 public:
  // underlying expression impl moves path
  std::unique_ptr<CelExpressionFlatImpl> GetExpression(ExecutionPath&& path) {
    bool unknowns;
    bool unknown_function_results;
    switch (GetParam()) {
      case UnknownProcessingOptions::kAttributeAndFunction:
        unknowns = true;
        unknown_function_results = true;
        break;
      case UnknownProcessingOptions::kAttributeOnly:
        unknowns = true;
        unknown_function_results = false;
        break;
      case UnknownProcessingOptions::kDisabled:
        unknowns = false;
        unknown_function_results = false;
        break;
    }
    return absl::make_unique<CelExpressionFlatImpl>(
        &dummy_expr_, std::move(path), 0, std::set<std::string>(), unknowns,
        unknown_function_results);
  }

 private:
  Expr dummy_expr_;
};

TEST_P(FunctionStepTest, SimpleFunctionTest) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call call2 = ConstFunction::MakeCall("Const2");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(5));
}

TEST_P(FunctionStepTest, TestStackUnderflow) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  AddFunction add_func;

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  EXPECT_FALSE(status.ok());
}

// Test that creation fails if fail on warnings is set in the warnings
// collection.
TEST(FunctionStepTest, TestNoOverloadsOnCreation) {
  CelFunctionRegistry registry;
  BuilderWarnings warnings(true);

  Expr::Call call = ConstFunction::MakeCall("Const0");

  // function step with empty overloads
  auto step0_status =
      CreateFunctionStep(&call, GetExprId(), registry, &warnings);

  EXPECT_FALSE(step0_status.ok());
}

// Test that no overloads error is warned, actual error delayed to runtime by
// default.
TEST_P(FunctionStepTest, TestNoOverloadsOnCreationDelayedError) {
  CelFunctionRegistry registry;
  ExecutionPath path;
  Expr::Call call = ConstFunction::MakeCall("Const0");
  BuilderWarnings warnings;

  // function step with empty overloads
  auto step0_status =
      CreateFunctionStep(&call, GetExprId(), registry, &warnings);

  EXPECT_TRUE(step0_status.ok());
  EXPECT_THAT(warnings.warnings(), testing::SizeIs(1));

  path.push_back(std::move(step0_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();
  ASSERT_TRUE(value.IsError());
}

// Test situation when no overloads match input arguments during evaluation.
TEST_P(FunctionStepTest, TestNoMatchingOverloadsDuringEvaluation) {
  ExecutionPath path;
  BuilderWarnings warnings;

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

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();
  ASSERT_TRUE(value.IsError());
}

// Test situation when no overloads match input arguments during evaluation
// and at least one of arguments is error.
TEST_P(FunctionStepTest,
       TestNoMatchingOverloadsDuringEvaluationErrorForwarding) {
  ExecutionPath path;
  BuilderWarnings warnings;

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

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(value.ErrorOrDie(), Eq(&error0));
}

TEST_P(FunctionStepTest, LazyFunctionTest) {
  ExecutionPath path;
  Activation activation;
  CelFunctionRegistry registry;
  BuilderWarnings warnings;

  auto register0_status =
      registry.RegisterLazyFunction(ConstFunction::CreateDescriptor("Const3"));
  ASSERT_OK(register0_status);
  auto insert0_status = activation.InsertFunction(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3"));
  ASSERT_OK(insert0_status);
  auto register1_status =
      registry.RegisterLazyFunction(ConstFunction::CreateDescriptor("Const2"));
  ASSERT_OK(register1_status);
  auto insert1_status = activation.InsertFunction(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2"));
  ASSERT_OK(insert1_status);
  ASSERT_OK(registry.Register(absl::make_unique<AddFunction>()));

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call call2 = ConstFunction::MakeCall("Const2");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(5));
}

// Test situation when no overloads match input arguments during evaluation
// and at least one of arguments is error.
TEST_P(FunctionStepTest,
       TestNoMatchingOverloadsDuringEvaluationErrorForwardingLazy) {
  ExecutionPath path;
  Activation activation;
  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  BuilderWarnings warnings;

  AddDefaults(registry);

  CelError error0;
  CelError error1;

  // Constants have ERROR type, while AddFunction expects INT.
  auto register0_status = registry.RegisterLazyFunction(
      ConstFunction::CreateDescriptor("ConstError1"));
  ASSERT_OK(register0_status);
  auto insert0_status =
      activation.InsertFunction(absl::make_unique<ConstFunction>(
          CelValue::CreateError(&error0), "ConstError1"));
  ASSERT_OK(insert0_status);
  auto register1_status = registry.RegisterLazyFunction(
      ConstFunction::CreateDescriptor("ConstError2"));
  ASSERT_OK(register1_status);
  auto insert1_status =
      activation.InsertFunction(absl::make_unique<ConstFunction>(
          CelValue::CreateError(&error1), "ConstError2"));
  ASSERT_OK(insert1_status);

  Expr::Call call1 = ConstFunction::MakeCall("ConstError1");
  Expr::Call call2 = ConstFunction::MakeCall("ConstError2");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(value.ErrorOrDie(), Eq(&error0));
}

std::string TestNameFn(testing::TestParamInfo<UnknownProcessingOptions> opt) {
  switch (opt.param) {
    case UnknownProcessingOptions::kDisabled:
      return "disabled";
    case UnknownProcessingOptions::kAttributeOnly:
      return "attribute_only";
    case UnknownProcessingOptions::kAttributeAndFunction:
      return "attribute_and_function";
  }
  return "";
}

INSTANTIATE_TEST_SUITE_P(
    UnknownSupport, FunctionStepTest,
    testing::Values(UnknownProcessingOptions::kDisabled,
                    UnknownProcessingOptions::kAttributeOnly,
                    UnknownProcessingOptions::kAttributeAndFunction),
    &TestNameFn);

class FunctionStepTestUnknowns
    : public testing::TestWithParam<UnknownProcessingOptions> {
 public:
  std::unique_ptr<CelExpressionFlatImpl> GetExpression(ExecutionPath&& path) {
    bool unknown_functions;
    switch (GetParam()) {
      case UnknownProcessingOptions::kAttributeAndFunction:
        unknown_functions = true;
        break;
      default:
        unknown_functions = false;
        break;
    }
    return absl::make_unique<CelExpressionFlatImpl>(&expr, std::move(path), 0,
                                                    std::set<std::string>(),
                                                    true, unknown_functions);
  }

 private:
  Expr expr;
};

TEST_P(FunctionStepTestUnknowns, PassedUnknownTest) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call call2 = ConstFunction::MakeCall("ConstUnknown");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsUnknownSet());
}

TEST_P(FunctionStepTestUnknowns, PartialUnknownHandlingTest) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  // Build the expression path that corresponds to CEL expression
  // "sink(param)".
  Expr::Ident ident1;
  ident1.set_name("param");
  Expr::Call call1 = SinkFunction::MakeCall();

  auto step0_status = CreateIdentStep(&ident1, GetExprId());
  auto step1_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  TestMessage msg;
  google::protobuf::Arena arena;
  activation.InsertValue("param", CelValue::CreateMessage(&msg, &arena));
  CelAttributePattern pattern(
      "param",
      {CelAttributeQualifierPattern::Create(CelValue::CreateBool(true))});

  // Set attribute pattern that marks attribute "param[true]" as unknown.
  // It should result in "param" being handled as partially unknown, which is
  // is handled as fully unknown when used as function input argument.
  activation.set_unknown_attribute_patterns({pattern});

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsUnknownSet());
}

TEST_P(FunctionStepTestUnknowns, UnknownVsErrorPrecedenceTest) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  CelError error0;
  CelValue error_value = CelValue::CreateError(&error0);

  ASSERT_TRUE(
      registry
          .Register(absl::make_unique<ConstFunction>(error_value, "ConstError"))
          .ok());

  Expr::Call call1 = ConstFunction::MakeCall("ConstError");
  Expr::Call call2 = ConstFunction::MakeCall("ConstUnknown");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl->Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsError());
  // Making sure we propagate the error.
  ASSERT_EQ(value.ErrorOrDie(), error_value.ErrorOrDie());
}

INSTANTIATE_TEST_SUITE_P(
    UnknownFunctionSupport, FunctionStepTestUnknowns,
    testing::Values(UnknownProcessingOptions::kAttributeOnly,
                    UnknownProcessingOptions::kAttributeAndFunction),
    &TestNameFn);

MATCHER_P2(IsAdd, a, b, "") {
  const UnknownFunctionResult* result = arg;
  return result->arguments().size() == 2 &&
         result->arguments().at(0).IsInt64() &&
         result->arguments().at(1).IsInt64() &&
         result->arguments().at(0).Int64OrDie() == a &&
         result->arguments().at(1).Int64OrDie() == b &&
         result->descriptor().name() == "_+_";
}

TEST(FunctionStepTestUnknownFunctionResults, CaptureArgs) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;

  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(registry.Register(
      absl::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  Expr::Call call1 = ConstFunction::MakeCall("Const2");
  Expr::Call call2 = ConstFunction::MakeCall("Const3");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsUnknownSet());
  // Arguments captured.
  EXPECT_THAT(value.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              ElementsAre(IsAdd(2, 3)));
}

TEST(FunctionStepTestUnknownFunctionResults, MergeDownCaptureArgs) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;

  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(registry.Register(
      absl::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  // Add(Add(2, 3), Add(2, 3))

  Expr::Call call1 = ConstFunction::MakeCall("Const2");
  Expr::Call call2 = ConstFunction::MakeCall("Const3");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);
  auto step3_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step4_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step5_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);
  auto step6_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);
  ASSERT_OK(step3_status);
  ASSERT_OK(step4_status);
  ASSERT_OK(step5_status);
  ASSERT_OK(step6_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));
  path.push_back(std::move(step3_status.value()));
  path.push_back(std::move(step4_status.value()));
  path.push_back(std::move(step5_status.value()));
  path.push_back(std::move(step6_status.value()));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsUnknownSet());
  // Arguments captured.
  EXPECT_THAT(value.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              ElementsAre(IsAdd(2, 3)));
}

TEST(FunctionStepTestUnknownFunctionResults, MergeCaptureArgs) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;

  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(registry.Register(
      absl::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  // Add(Add(2, 3), Add(3, 2))

  Expr::Call call1 = ConstFunction::MakeCall("Const2");
  Expr::Call call2 = ConstFunction::MakeCall("Const3");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);
  auto step3_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step4_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step5_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);
  auto step6_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);
  ASSERT_OK(step3_status);
  ASSERT_OK(step4_status);
  ASSERT_OK(step5_status);
  ASSERT_OK(step6_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));
  path.push_back(std::move(step3_status.value()));
  path.push_back(std::move(step4_status.value()));
  path.push_back(std::move(step5_status.value()));
  path.push_back(std::move(step6_status.value()));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsUnknownSet()) << value.ErrorOrDie()->ToString();
  // Arguments captured.
  EXPECT_THAT(value.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              UnorderedElementsAre(IsAdd(2, 3), IsAdd(3, 2)));
}

TEST(FunctionStepTestUnknownFunctionResults, UnknownVsErrorPrecedenceTest) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;

  CelError error0;
  CelValue error_value = CelValue::CreateError(&error0);
  UnknownSet unknown_set;
  CelValue unknown_value = CelValue::CreateUnknownSet(&unknown_set);

  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(error_value, "ConstError")));
  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(unknown_value, "ConstUnknown")));
  ASSERT_OK(registry.Register(
      absl::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  Expr::Call call1 = ConstFunction::MakeCall("ConstError");
  Expr::Call call2 = ConstFunction::MakeCall("ConstUnknown");
  Expr::Call add_call = AddFunction::MakeCall();

  auto step0_status =
      CreateFunctionStep(&call1, GetExprId(), registry, &warnings);
  auto step1_status =
      CreateFunctionStep(&call2, GetExprId(), registry, &warnings);
  auto step2_status =
      CreateFunctionStep(&add_call, GetExprId(), registry, &warnings);

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);
  ASSERT_OK(step2_status);

  path.push_back(std::move(step0_status.value()));
  path.push_back(std::move(step1_status.value()));
  path.push_back(std::move(step2_status.value()));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  ASSERT_OK(status);

  auto value = status.value();

  ASSERT_TRUE(value.IsError());
  // Making sure we propagate the error.
  ASSERT_EQ(value.ErrorOrDie(), error_value.ErrorOrDie());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
