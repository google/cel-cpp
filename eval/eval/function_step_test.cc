#include "eval/eval/function_step.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/descriptor.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/test_type_registry.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "eval/public/unknown_function_result_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ast::internal::Call;
using ::cel::ast::internal::Expr;
using ::cel::ast::internal::Ident;
using testing::ElementsAre;
using testing::Eq;
using testing::Not;
using testing::UnorderedElementsAre;
using cel::internal::IsOk;
using cel::internal::StatusIs;

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

  static Call MakeCall(absl::string_view name) {
    Call call;
    call.set_function(std::string(name));
    call.set_target(nullptr);
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

  static Call MakeCall() {
    Call call;
    call.set_function("_+_");
    call.mutable_args().emplace_back();
    call.mutable_args().emplace_back();
    call.set_target(nullptr);
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
  explicit SinkFunction(CelValue::Type type, bool is_strict = true)
      : CelFunction(CreateDescriptor(type, is_strict)) {}

  static CelFunctionDescriptor CreateDescriptor(CelValue::Type type,
                                                bool is_strict = true) {
    return CelFunctionDescriptor{"Sink", false, {type}, is_strict};
  }

  static Call MakeCall() {
    Call call;
    call.set_function("Sink");
    call.mutable_args().emplace_back();
    call.set_target(nullptr);
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

std::vector<CelValue::Type> ArgumentMatcher(const Call& call) {
  return ArgumentMatcher(call.has_target() ? call.args().size() + 1
                                           : call.args().size());
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> MakeTestFunctionStep(
    const Call& call, const CelFunctionRegistry& registry) {
  auto argument_matcher = ArgumentMatcher(call);
  auto lazy_overloads = registry.FindLazyOverloads(
      call.function(), call.has_target(), argument_matcher);
  if (!lazy_overloads.empty()) {
    return CreateFunctionStep(call, GetExprId(), lazy_overloads);
  }
  auto overloads = registry.FindOverloads(call.function(), call.has_target(),
                                          argument_matcher);
  return CreateFunctionStep(call, GetExprId(), overloads);
}

// Test common functions with varying levels of unknown support.
class FunctionStepTest
    : public testing::TestWithParam<UnknownProcessingOptions> {
 public:
  // underlying expression impl moves path
  std::unique_ptr<CelExpressionFlatImpl> GetExpression(ExecutionPath&& path) {
    bool unknowns = false;
    bool unknown_function_results = false;
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
        &dummy_expr_, std::move(path), &TestTypeRegistry(), 0,
        std::set<std::string>(), unknowns, unknown_function_results);
  }

 private:
  Expr dummy_expr_;
};

TEST_P(FunctionStepTest, SimpleFunctionTest) {
  ExecutionPath path;
  BuilderWarnings warnings;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  Call call1 = ConstFunction::MakeCall("Const3");
  Call call2 = ConstFunction::MakeCall("Const2");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

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

  Call call1 = ConstFunction::MakeCall("Const3");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

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

  Call call1 = ConstFunction::MakeCall("Const3");
  Call call2 = ConstFunction::MakeCall("Const4");
  // Add expects {int64_t, int64_t} but it's {int64_t, uint64_t}.
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  std::unique_ptr<CelExpressionFlatImpl> impl = GetExpression(std::move(path));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl->Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(*value.ErrorOrDie(),
              StatusIs(absl::StatusCode::kUnknown,
                       testing::HasSubstr("_+_(int64, uint64)")));
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

  Call call1 = ConstFunction::MakeCall("ConstError1");
  Call call2 = ConstFunction::MakeCall("ConstError2");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

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

  Call call1 = ConstFunction::MakeCall("Const3");
  Call call2 = ConstFunction::MakeCall("Const2");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

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

  Call call1 = ConstFunction::MakeCall("ConstError1");
  Call call2 = ConstFunction::MakeCall("ConstError2");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

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
    return absl::make_unique<CelExpressionFlatImpl>(
        &expr_, std::move(path), &TestTypeRegistry(), 0,
        std::set<std::string>(), true, unknown_functions);
  }

 private:
  Expr expr_;
};

TEST_P(FunctionStepTestUnknowns, PassedUnknownTest) {
  ExecutionPath path;

  CelFunctionRegistry registry;
  AddDefaults(registry);

  Call call1 = ConstFunction::MakeCall("Const3");
  Call call2 = ConstFunction::MakeCall("ConstUnknown");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

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
  Ident ident1;
  ident1.set_name("param");
  Call call1 = SinkFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident1, GetExprId()));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call1, registry));

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

  Call call1 = ConstFunction::MakeCall("ConstError");
  Call call2 = ConstFunction::MakeCall("ConstUnknown");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

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

TEST(FunctionStepTestUnknownFunctionResults, CaptureArgs) {
  ExecutionPath path;
  CelFunctionRegistry registry;

  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(2), "Const2")));
  ASSERT_OK(registry.Register(
      absl::make_unique<ConstFunction>(CelValue::CreateInt64(3), "Const3")));
  ASSERT_OK(registry.Register(
      absl::make_unique<AddFunction>(ShouldReturnUnknown::kYes)));

  Call call1 = ConstFunction::MakeCall("Const2");
  Call call2 = ConstFunction::MakeCall("Const3");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
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

  Call call1 = ConstFunction::MakeCall("Const2");
  Call call2 = ConstFunction::MakeCall("Const3");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step3, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step4, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step5, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step6, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));
  path.push_back(std::move(step4));
  path.push_back(std::move(step5));
  path.push_back(std::move(step6));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
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

  Call call1 = ConstFunction::MakeCall("Const2");
  Call call2 = ConstFunction::MakeCall("Const3");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step3, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step4, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step5, MakeTestFunctionStep(add_call, registry));
  ASSERT_OK_AND_ASSIGN(auto step6, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  path.push_back(std::move(step3));
  path.push_back(std::move(step4));
  path.push_back(std::move(step5));
  path.push_back(std::move(step6));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet()) << *(value.ErrorOrDie());
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

  Call call1 = ConstFunction::MakeCall("ConstError");
  Call call2 = ConstFunction::MakeCall("ConstUnknown");
  Call add_call = AddFunction::MakeCall();

  ASSERT_OK_AND_ASSIGN(auto step0, MakeTestFunctionStep(call1, registry));
  ASSERT_OK_AND_ASSIGN(auto step1, MakeTestFunctionStep(call2, registry));
  ASSERT_OK_AND_ASSIGN(auto step2, MakeTestFunctionStep(add_call, registry));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));

  Expr dummy_expr;

  CelExpressionFlatImpl impl(&dummy_expr, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true);

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsError());
  // Making sure we propagate the error.
  ASSERT_EQ(value.ErrorOrDie(), error_value.ErrorOrDie());
}

class MessageFunction : public CelFunction {
 public:
  MessageFunction()
      : CelFunction(
            CelFunctionDescriptor("Fn", false, {CelValue::Type::kMessage})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 1 || !args.at(0).IsMessage()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = CelValue::CreateStringView("message");
    return absl::OkStatus();
  }
};

class MessageIdentityFunction : public CelFunction {
 public:
  MessageIdentityFunction()
      : CelFunction(
            CelFunctionDescriptor("Fn", false, {CelValue::Type::kMessage})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 1 || !args.at(0).IsMessage()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = args.at(0);
    return absl::OkStatus();
  }
};

class NullFunction : public CelFunction {
 public:
  NullFunction()
      : CelFunction(
            CelFunctionDescriptor("Fn", false, {CelValue::Type::kNullType})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 1 || args.at(0).type() != CelValue::Type::kNullType) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }

    *result = CelValue::CreateStringView("null");
    return absl::OkStatus();
  }
};

// Setup for a simple evaluation plan that runs 'Fn(id)'.
class FunctionStepNullCoercionTest : public testing::Test {
 public:
  FunctionStepNullCoercionTest() {
    identifier_expr_.set_id(GetExprId());
    identifier_expr_.mutable_ident_expr().set_name("id");
    call_expr_.set_id(GetExprId());
    call_expr_.mutable_call_expr().set_function("Fn");
    call_expr_.mutable_call_expr().mutable_args().emplace_back().set_id(
        GetExprId());
    activation_.InsertValue("id", CelValue::CreateNull());
  }

 protected:
  Expr dummy_expr_;
  Expr identifier_expr_;
  Expr call_expr_;
  Activation activation_;
  google::protobuf::Arena arena_;
  CelFunctionRegistry registry_;
};

TEST_F(FunctionStepNullCoercionTest, EnabledSupportsMessageOverloads) {
  ExecutionPath path;
  ASSERT_OK(registry_.Register(std::make_unique<MessageFunction>()));

  ASSERT_OK_AND_ASSIGN(
      auto ident_step,
      CreateIdentStep(identifier_expr_.ident_expr(), identifier_expr_.id()));
  path.push_back(std::move(ident_step));

  ASSERT_OK_AND_ASSIGN(auto call_step,
                       MakeTestFunctionStep(call_expr_.call_expr(), registry_));

  path.push_back(std::move(call_step));

  CelExpressionFlatImpl impl(&dummy_expr_, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true, true,
                             /*enable_null_coercion=*/true);

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation_, &arena_));
  ASSERT_TRUE(value.IsString());
  ASSERT_THAT(value.StringOrDie().value(), testing::Eq("message"));
}

TEST_F(FunctionStepNullCoercionTest, EnabledPrefersNullOverloads) {
  ExecutionPath path;
  ASSERT_OK(registry_.Register(std::make_unique<MessageFunction>()));
  ASSERT_OK(registry_.Register(std::make_unique<NullFunction>()));

  ASSERT_OK_AND_ASSIGN(
      auto ident_step,
      CreateIdentStep(identifier_expr_.ident_expr(), identifier_expr_.id()));
  path.push_back(std::move(ident_step));

  ASSERT_OK_AND_ASSIGN(auto call_step,
                       MakeTestFunctionStep(call_expr_.call_expr(), registry_));

  path.push_back(std::move(call_step));

  CelExpressionFlatImpl impl(&dummy_expr_, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true, true,
                             /*enable_null_coercion=*/true);

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation_, &arena_));
  ASSERT_TRUE(value.IsString());
  ASSERT_THAT(value.StringOrDie().value(), testing::Eq("null"));
}

TEST_F(FunctionStepNullCoercionTest, EnabledNullMessageDoesNotEscape) {
  ExecutionPath path;
  ASSERT_OK(registry_.Register(std::make_unique<MessageIdentityFunction>()));

  ASSERT_OK_AND_ASSIGN(
      auto ident_step,
      CreateIdentStep(identifier_expr_.ident_expr(), identifier_expr_.id()));
  path.push_back(std::move(ident_step));

  ASSERT_OK_AND_ASSIGN(auto call_step,
                       MakeTestFunctionStep(call_expr_.call_expr(), registry_));

  path.push_back(std::move(call_step));

  CelExpressionFlatImpl impl(&dummy_expr_, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true, true,
                             /*enable_null_coercion=*/true);

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation_, &arena_));
  ASSERT_TRUE(value.IsNull());
  ASSERT_FALSE(value.IsMessage());
}

TEST_F(FunctionStepNullCoercionTest, Disabled) {
  ExecutionPath path;
  ASSERT_OK(registry_.Register(std::make_unique<MessageFunction>()));

  ASSERT_OK_AND_ASSIGN(
      auto ident_step,
      CreateIdentStep(identifier_expr_.ident_expr(), identifier_expr_.id()));
  path.push_back(std::move(ident_step));

  ASSERT_OK_AND_ASSIGN(auto call_step,
                       MakeTestFunctionStep(call_expr_.call_expr(), registry_));

  path.push_back(std::move(call_step));

  CelExpressionFlatImpl impl(&dummy_expr_, std::move(path), &TestTypeRegistry(),
                             0, {}, true, true, true,
                             /*enable_null_coercion=*/false);

  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation_, &arena_));
  ASSERT_TRUE(value.IsError());
}

TEST(FunctionStepStrictnessTest,
     IfFunctionStrictAndGivenUnknownSkipsInvocation) {
  UnknownSet unknown_set;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.Register(absl::make_unique<ConstFunction>(
      CelValue::CreateUnknownSet(&unknown_set), "ConstUnknown")));
  ASSERT_OK(registry.Register(std::make_unique<SinkFunction>(
      CelValue::Type::kUnknownSet, /*is_strict=*/true)));
  ExecutionPath path;
  Call call0 = ConstFunction::MakeCall("ConstUnknown");
  Call call1 = SinkFunction::MakeCall();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step0,
                       MakeTestFunctionStep(call0, registry));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step1,
                       MakeTestFunctionStep(call1, registry));
  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  Expr placeholder_expr;
  CelExpressionFlatImpl impl(&placeholder_expr, std::move(path),
                             &TestTypeRegistry(), 0, {}, true, true);
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_TRUE(value.IsUnknownSet());
}

TEST(FunctionStepStrictnessTest, IfFunctionNonStrictAndGivenUnknownInvokesIt) {
  UnknownSet unknown_set;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.Register(absl::make_unique<ConstFunction>(
      CelValue::CreateUnknownSet(&unknown_set), "ConstUnknown")));
  ASSERT_OK(registry.Register(std::make_unique<SinkFunction>(
      CelValue::Type::kUnknownSet, /*is_strict=*/false)));
  ExecutionPath path;
  Call call0 = ConstFunction::MakeCall("ConstUnknown");
  Call call1 = SinkFunction::MakeCall();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step0,
                       MakeTestFunctionStep(call0, registry));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExpressionStep> step1,
                       MakeTestFunctionStep(call1, registry));
  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  Expr placeholder_expr;
  CelExpressionFlatImpl impl(&placeholder_expr, std::move(path),
                             &TestTypeRegistry(), 0, {}, true, true);
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue value, impl.Evaluate(activation, &arena));
  ASSERT_THAT(value, test::IsCelInt64(Eq(0)));
}

}  // namespace
}  // namespace google::api::expr::runtime
