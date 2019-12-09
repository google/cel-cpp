#include "eval/eval/function_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/strings/str_cat.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_function_provider.h"
#include "eval/public/cel_function_registry.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

// Implementation of ExpressionStep that finds suitable CelFunction overload and
// invokes it. Abstract base class standardizes behavior between lazy and eager
// function bindings. Derived classes provide ResolveFunction behavior.
class AbstractFunctionStep : public ExpressionStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  AbstractFunctionStep(size_t num_arguments, int64_t expr_id)
      : ExpressionStepBase(expr_id), num_arguments_(num_arguments) {}

  cel_base::Status Evaluate(ExecutionFrame* frame) const override;
  virtual cel_base::StatusOr<const CelFunction*> ResolveFunction(
      absl::Span<const CelValue> args, const ExecutionFrame* frame) const = 0;

 protected:
  size_t num_arguments_;
};

cel_base::Status AbstractFunctionStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(num_arguments_)) {
    return cel_base::Status(cel_base::StatusCode::kInternal, "Value stack underflow");
  }

  // Create Span object that contains input arguments to the function.
  auto input_args = frame->value_stack().GetSpan(num_arguments_);

  // Derived class resolves to a single function overload or none.
  auto status = ResolveFunction(input_args, frame);
  if (!status.ok()) {
    return status.status();
  }
  const CelFunction* matched_function = status.ValueOrDie();

  CelValue result = CelValue::CreateNull();

  // Overload found
  if (matched_function != nullptr) {
    cel_base::Status status =
        matched_function->Evaluate(input_args, &result, frame->arena());
    if (!status.ok()) {
      return status;
    }
  } else {
    // No matching overloads.
    // We should not treat absense of overloads as non-recoverable error.
    // Such absence can be caused by presence of CelError in arguments.
    // To enable behavior of functions that accept CelError( &&, || ), CelErrors
    // should be propagated along execution path.
    for (const CelValue& arg : input_args) {
      if (arg.IsError()) {
        result = arg;
        break;
      }
    }
    // If no errors in input args, create new CelError.
    if (!result.IsError()) {
      result = CreateNoMatchingOverloadError(frame->arena());
    }
  }

  frame->value_stack().Pop(input_args.length());
  frame->value_stack().Push(result);

  return cel_base::OkStatus();
}

class EagerFunctionStep : public AbstractFunctionStep {
 public:
  EagerFunctionStep(std::vector<const CelFunction*>&& overloads,
                    size_t num_args, int64_t expr_id)
      : AbstractFunctionStep(num_args, expr_id), overloads_(overloads) {}

  cel_base::StatusOr<const CelFunction*> ResolveFunction(
      absl::Span<const CelValue> input_args,
      const ExecutionFrame* frame) const override;

 private:
  std::vector<const CelFunction*> overloads_;
};

cel_base::StatusOr<const CelFunction*> EagerFunctionStep::ResolveFunction(
    absl::Span<const CelValue> input_args, const ExecutionFrame* frame) const {
  const CelFunction* matched_function = nullptr;

  for (auto overload : overloads_) {
    if (overload->MatchArguments(input_args)) {
      // More than one overload matches our arguments.
      if (matched_function != nullptr) {
        return cel_base::Status(cel_base::StatusCode::kInternal,
                            "Cannot resolve overloads");
      }

      matched_function = overload;
    }
  }
  return matched_function;
}

class LazyFunctionStep : public AbstractFunctionStep {
 public:
  // Constructs LazyFunctionStep that attempts to lookup function implementation
  // at runtime.
  LazyFunctionStep(const std::string& name, size_t num_args,
                   bool receiver_style,
                   const std::vector<const CelFunctionProvider*>& providers,
                   int64_t expr_id)
      : AbstractFunctionStep(num_args, expr_id),
        name_(name),
        receiver_style_(receiver_style),
        providers_(providers) {}

  cel_base::StatusOr<const CelFunction*> ResolveFunction(
      absl::Span<const CelValue> input_args,
      const ExecutionFrame* frame) const override;

 private:
  std::string name_;
  bool receiver_style_;
  std::vector<const CelFunctionProvider*> providers_;
};

cel_base::StatusOr<const CelFunction*> LazyFunctionStep::ResolveFunction(
    absl::Span<const CelValue> input_args, const ExecutionFrame* frame) const {
  const CelFunction* matched_function = nullptr;

  std::vector<CelValue::Type> arg_types(num_arguments_);

  std::transform(input_args.begin(), input_args.end(), arg_types.begin(),
                 [](const CelValue& value) { return value.type(); });

  CelFunctionDescriptor matcher{name_, receiver_style_, arg_types};

  const BaseActivation& activation = frame->activation();
  for (auto provider : providers_) {
    auto status = provider->GetFunction(matcher, activation);
    if (!status.ok()) {
      return status;
    }
    auto overload = status.ValueOrDie();
    if (overload != nullptr && overload->MatchArguments(input_args)) {
      // More than one overload matches our arguments.
      if (matched_function != nullptr) {
        return cel_base::Status(cel_base::StatusCode::kInternal,
                            "Cannot resolve overloads");
      }

      matched_function = overload;
    }
  }

  return matched_function;
}

}  // namespace

cel_base::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const google::api::expr::v1alpha1::Expr::Call* call_expr, int64_t expr_id,
    const CelFunctionRegistry& function_registry) {
  bool receiver_style = call_expr->has_target();
  size_t num_args = call_expr->args_size() + (receiver_style ? 1 : 0);
  const std::string& name = call_expr->function();

  std::vector<CelValue::Type> args(num_args, CelValue::Type::kAny);

  std::vector<const CelFunctionProvider*> lazy_overloads =
      function_registry.FindLazyOverloads(name, receiver_style, args);

  if (!lazy_overloads.empty()) {
    std::unique_ptr<ExpressionStep> step = absl::make_unique<LazyFunctionStep>(
        name, num_args, receiver_style, lazy_overloads, expr_id);
    return std::move(step);
  }

  auto overloads = function_registry.FindOverloads(name, receiver_style, args);

  if (!overloads.empty()) {
    std::unique_ptr<ExpressionStep> step = absl::make_unique<EagerFunctionStep>(
        std::move(overloads), num_args, expr_id);
    return std::move(step);
  }

  // No overloads found.
  return ::cel_base::Status(cel_base::StatusCode::kInvalidArgument,
                        "No overloads provided for FunctionStep creation");
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
