#include "eval/eval/function_step.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/activation.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_provider.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_function_result_set.h"
#include "eval/public/unknown_set.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

// Non-strict functions are allowed to consume errors and UnknownSets. Currently
// only the special function "@not_strictly_false" is allowed to do this.
bool IsNonStrict(const std::string& name) {
  return (name == builtin::kNotStrictlyFalse ||
          name == builtin::kNotStrictlyFalseDeprecated);
}

// Determine if the overload should be considered. Overloads that can consume
// errors or unknown sets must be allowed as a non-strict function.
bool ShouldAcceptOverload(const CelFunction* function,
                          absl::Span<const CelValue> arguments) {
  if (function == nullptr) {
    return false;
  }
  for (size_t i = 0; i < arguments.size(); i++) {
    if (arguments[i].IsUnknownSet() || arguments[i].IsError()) {
      return IsNonStrict(function->descriptor().name());
    }
  }
  return true;
}

// Convert partially unknown arguments to unknowns before passing to the
// function.
// TODO(issues/52): See if this can be refactored to remove the eager
// arguments copy.
// Argument and attribute spans are expected to be equal length.
std::vector<CelValue> CheckForPartialUnknowns(
    ExecutionFrame* frame, absl::Span<const CelValue> args,
    absl::Span<const AttributeTrail> attrs) {
  std::vector<CelValue> result;
  result.reserve(args.size());
  for (size_t i = 0; i < args.size(); i++) {
    auto attr_set = frame->attribute_utility().CheckForUnknowns(
        attrs.subspan(i, 1), /*use_partial=*/true);
    if (!attr_set.attributes().empty()) {
      auto unknown_set = google::protobuf::Arena::Create<UnknownSet>(frame->arena(),
                                                           std::move(attr_set));
      result.push_back(CelValue::CreateUnknownSet(unknown_set));
    } else {
      result.push_back(args.at(i));
    }
  }

  return result;
}

// Implementation of ExpressionStep that finds suitable CelFunction overload and
// invokes it. Abstract base class standardizes behavior between lazy and eager
// function bindings. Derived classes provide ResolveFunction behavior.
class AbstractFunctionStep : public ExpressionStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  AbstractFunctionStep(const std::string& name, size_t num_arguments,
                       int64_t expr_id)
      : ExpressionStepBase(expr_id),
        name_(name),
        num_arguments_(num_arguments) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

  absl::Status DoEvaluate(ExecutionFrame* frame, CelValue* result) const;

  virtual absl::StatusOr<const CelFunction*> ResolveFunction(
      absl::Span<const CelValue> args, const ExecutionFrame* frame) const = 0;

 protected:
  std::string name_;
  size_t num_arguments_;
};

absl::Status AbstractFunctionStep::DoEvaluate(ExecutionFrame* frame,
                                              CelValue* result) const {
  // Create Span object that contains input arguments to the function.
  auto input_args = frame->value_stack().GetSpan(num_arguments_);

  std::vector<CelValue> unknowns_args;
  // Preprocess args. If an argument is partially unknown, convert it to an
  // unknown attribute set.
  if (frame->enable_unknowns()) {
    auto input_attrs = frame->value_stack().GetAttributeSpan(num_arguments_);
    unknowns_args = CheckForPartialUnknowns(frame, input_args, input_attrs);
    input_args = absl::MakeConstSpan(unknowns_args);
  }

  // Derived class resolves to a single function overload or none.
  auto status = ResolveFunction(input_args, frame);
  if (!status.ok()) {
    return status.status();
  }
  const CelFunction* matched_function = status.value();

  // Overload found and is allowed to consume the arguments.
  if (ShouldAcceptOverload(matched_function, input_args)) {
    absl::Status status =
        matched_function->Evaluate(input_args, result, frame->arena());
    if (!status.ok()) {
      return status;
    }
    if (frame->enable_unknown_function_results() &&
        IsUnknownFunctionResult(*result)) {
      const auto* function_result =
          google::protobuf::Arena::Create<UnknownFunctionResult>(
              frame->arena(), matched_function->descriptor(), id(),
              std::vector<CelValue>(input_args.begin(), input_args.end()));
      const auto* unknown_set = google::protobuf::Arena::Create<UnknownSet>(
          frame->arena(), UnknownFunctionResultSet(function_result));
      *result = CelValue::CreateUnknownSet(unknown_set);
    }
  } else {
    // No matching overloads.
    // We should not treat absense of overloads as non-recoverable error.
    // Such absence can be caused by presence of CelError in arguments.
    // To enable behavior of functions that accept CelError( &&, || ), CelErrors
    // should be propagated along execution path.
    for (const CelValue& arg : input_args) {
      if (arg.IsError()) {
        *result = arg;
        return absl::OkStatus();
      }
    }

    if (frame->enable_unknowns()) {
      // Already converted partial unknowns to unknown sets so just merge.
      auto unknown_set =
          frame->attribute_utility().MergeUnknowns(input_args, nullptr);
      if (unknown_set != nullptr) {
        *result = CelValue::CreateUnknownSet(unknown_set);
        return absl::OkStatus();
      }
    }

    // If no errors or unknowns in input args, create new CelError.
    *result = CreateNoMatchingOverloadError(frame->arena());
  }

  return absl::OkStatus();
}

absl::Status AbstractFunctionStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(num_arguments_)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  CelValue result;
  auto status = DoEvaluate(frame, &result);
  if (!status.ok()) {
    return status;
  }

  frame->value_stack().Pop(num_arguments_);
  frame->value_stack().Push(result);

  return absl::OkStatus();
}

class EagerFunctionStep : public AbstractFunctionStep {
 public:
  EagerFunctionStep(std::vector<const CelFunction*>&& overloads,
                    const std::string& name, size_t num_args, int64_t expr_id)
      : AbstractFunctionStep(name, num_args, expr_id), overloads_(overloads) {}

  absl::StatusOr<const CelFunction*> ResolveFunction(
      absl::Span<const CelValue> input_args,
      const ExecutionFrame* frame) const override;

 private:
  std::vector<const CelFunction*> overloads_;
};

absl::StatusOr<const CelFunction*> EagerFunctionStep::ResolveFunction(
    absl::Span<const CelValue> input_args, const ExecutionFrame* frame) const {
  const CelFunction* matched_function = nullptr;

  for (auto overload : overloads_) {
    if (overload->MatchArguments(input_args)) {
      // More than one overload matches our arguments.
      if (matched_function != nullptr) {
        return absl::Status(absl::StatusCode::kInternal,
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
      : AbstractFunctionStep(name, num_args, expr_id),
        receiver_style_(receiver_style),
        providers_(providers) {}

  absl::StatusOr<const CelFunction*> ResolveFunction(
      absl::Span<const CelValue> input_args,
      const ExecutionFrame* frame) const override;

 private:
  bool receiver_style_;
  std::vector<const CelFunctionProvider*> providers_;
};

absl::StatusOr<const CelFunction*> LazyFunctionStep::ResolveFunction(
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
    auto overload = status.value();
    if (overload != nullptr && overload->MatchArguments(input_args)) {
      // More than one overload matches our arguments.
      if (matched_function != nullptr) {
        return absl::Status(absl::StatusCode::kInternal,
                            "Cannot resolve overloads");
      }

      matched_function = overload;
    }
  }

  return matched_function;
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const google::api::expr::v1alpha1::Expr::Call* call_expr, int64_t expr_id,
    const CelFunctionRegistry& function_registry,
    BuilderWarnings* builder_warnings) {
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

  // No overloads found.
  if (overloads.empty()) {
    RETURN_IF_ERROR(builder_warnings->AddWarning(
        absl::Status(absl::StatusCode::kInvalidArgument,
                     "No overloads provided for FunctionStep creation")));
  }

  std::unique_ptr<ExpressionStep> step = absl::make_unique<EagerFunctionStep>(
      std::move(overloads), name, num_args, expr_id);
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
