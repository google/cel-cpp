#include "eval/eval/function_step.h"

#include <memory>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/unknown_function_result_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::Not;
using testing::UnorderedElementsAre;
using cel::internal::IsOk;

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
    return CelFunctionDescriptor{name, false, {}};
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
  explicit SinkFunction(CelValue::Type type)
      : CelFunction(CreateDescriptor(type)) {}

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

std::vector<CelValue::Type> ArgumentMatcher(int argument_count) {
  std::vector<CelValue::Type> argument_matcher(argument_count);
  for (int i = 0; i < argument_count; i++) {
    argument_matcher[i] = CelValue::Type::kAny;
  }
  return argument_matcher;
}

std::vector<CelValue::Type> ArgumentMatcher(const Expr::Call* call) {
  return ArgumentMatcher(call->has_target() ? call->args_size() + 1
                                            : call->args_size());
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> MakeTestFunctionStep(
    const Expr::Call* call, const CelFunctionRegistry& registry) {
  auto argument_matcher = ArgumentMatcher(call);
  auto lazy_overloads = registry.FindLazyOverloads(
      call->function(), call->has_target(), argument_matcher);
  if (!lazy_overloads.empty()) {
    return CreateFunctionStep(call, GetExprId(), lazy_overloads);
  }
  auto overloads = registry.FindOverloads(call->function(), call->has_target(),
                                          argument_matcher);
  return CreateFunctionStep(call, GetExprId(), overloads);
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  EXPECT_THAT(impl->Evaluate(activation, &arena), Not(IsOk()));
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
}

// Test situation when no overloads match input arguments during evaluation
// and at least one of arguments is error.
TEST_P(FunctionStepTest,
       TestNoMatchingOverloadsDuringEvaluationErrorForwarding) {
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(value.ErrorOrDie(), Eq(&error0));
}

TEST_P(FunctionStepTest, LazyFunctionTest) {
  ExecutionPath path;
  Activation activation;
  CelFunctionRegistry registry;
  BuilderWarnings warnings;

  ASSERT_OK(
      registry.RegisterLazyFunction(ConstFunction::CreateDescriptor("Const3")));
  ASSERT_OK(activation.InsertFunction(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(
      registry.RegisterLazyFunction(ConstFunction::CreateDescriptor("Const2")));
  ASSERT_OK(activation.InsertFunction(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(absl::make_unique<AddFunction>()));

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call call2 = ConstFunction::MakeCall("Const2");
  Expr::Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
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

  AddDefaults(registry);

  CelError error0;
  CelError error1;

  // Constants have ERROR type, while AddFunction expects INT.
  ASSERT_OK(registry.RegisterLazyFunction(
      ConstFunction::CreateDescriptor("ConstError1")));
  ASSERT_OK(activation.InsertFunction(absl::make_unique<ConstFunction>(
      CelValue::CreateError(&error0), "ConstError1")));
  ASSERT_OK(registry.RegisterLazyFunction(
      ConstFunction::CreateDescriptor("ConstError2")));
  ASSERT_OK(activation.InsertFunction(absl::make_unique<ConstFunction>(
      CelValue::CreateError(&error1), "ConstError2")));

  Expr::Call call1 = ConstFunction::MakeCall("ConstError1");
  Expr::Call call2 = ConstFunction::MakeCall("ConstError2");
  Expr::Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
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
    return absl::make_unique<CelExpressionFlatImpl>(&expr_, std::move(path), 0,
                                                    std::set<std::string>(),
                                                    true, unknown_functions);
  }

 private:
  Expr expr_;
};

TEST_P(FunctionStepTestUnknowns, PassedUnknownTest) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  Expr::Call call1 = ConstFunction::MakeCall("Const3");
  Expr::Call call2 = ConstFunction::MakeCall("ConstUnknown");
  Expr::Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
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

  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(&ident1, GetExprId()));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call1, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  TestMessage msg;
  google::protobuf::Arena arena;
  activation.InsertValue("param", CelProtoWrapper::CreateMessage(&msg, &arena));
  CelAttributePattern pattern(
      "param",
      {CelAttributeQualifierPattern::Create(CelValue::CreateBool(true))});

  // Set attribute pattern that marks attribute "param[true]" as unknown.
  // It should result in "param" being handled as partially unknown, which is
  // is handled as fully unknown when used as function input argument.
  activation.set_unknown_attribute_patterns({pattern});

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
}

TEST_P(FunctionStepTestUnknowns, UnknownVsErrorPrecedenceTest) {
  ExecutionPath path;
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
  // Arguments captured.
  EXPECT_THAT(value.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              ElementsAre(IsAdd(2, 3)));
}

TEST(FunctionStepTestUnknownFunctionResults, MergeDownCaptureArgs) {
  ExecutionPath path;
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step3, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step4, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step5, MakeTestFunctionStep(&add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step6, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));
  path.push_back(std::move(step4));
  path.push_back(std::move(step5));
  path.push_back(std::move(step6));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
  // Arguments captured.
  EXPECT_THAT(value.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              ElementsAre(IsAdd(2, 3)));
}

TEST(FunctionStepTestUnknownFunctionResults, MergeCaptureArgs) {
  ExecutionPath path;
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step3, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step4, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step5, MakeTestFunctionStep(&add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step6, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));
  path.push_back(std::move(step4));
  path.push_back(std::move(step5));
  path.push_back(std::move(step6));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet()) << *(value.ErrorOrDie());
  // Arguments captured.
  EXPECT_THAT(value.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              UnorderedElementsAre(IsAdd(2, 3), IsAdd(3, 2)));
}

TEST(FunctionStepTestUnknownFunctionResults, UnknownVsErrorPrecedenceTest) {
  ExecutionPath path;
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

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(&call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(&call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(&add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), 0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  // Making sure we propagate the error.
  ASSERT_EQ(value.ErrorOrDie(), error_value.ErrorOrDie());
}

}  // namespace

}  // namespace google::api::expr::runtime
