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
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/function.h"
#include "base/function_interface.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/value.h"
#include "base/values/error_value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "eval/internal/interop.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_provider.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::FunctionEvaluationContext;
using ::cel::Handle;
using ::cel::Value;

// Determine if the overload should be considered. Overloads that can consume
// errors or unknown sets must be allowed as a non-strict function.
bool ShouldAcceptOverload(const cel::FunctionDescriptor& descriptor,
                          absl::Span<const cel::Handle<cel::Value>> arguments) {
  for (size_t i = 0; i < arguments.size(); i++) {
    if (arguments[i]->Is<cel::UnknownValue>() ||
        arguments[i]->Is<cel::ErrorValue>()) {
      return !descriptor.is_strict();
    }
  }
  return true;
}

bool ArgumentKindsMatch(const cel::FunctionDescriptor& descriptor,
                        absl::Span<const cel::Handle<cel::Value>> arguments) {
  auto types_size = descriptor.types().size();

  if (types_size != arguments.size()) {
    return false;
  }

  for (size_t i = 0; i < types_size; i++) {
    const auto& arg = arguments[i];
    cel::Kind param_kind = descriptor.types()[i];
    if (arg->kind() != param_kind && param_kind != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

// Convert partially unknown arguments to unknowns before passing to the
// function.
// TODO(issues/52): See if this can be refactored to remove the eager
// arguments copy.
// Argument and attribute spans are expected to be equal length.
std::vector<cel::Handle<cel::Value>> CheckForPartialUnknowns(
    ExecutionFrame* frame, absl::Span<const cel::Handle<cel::Value>> args,
    absl::Span<const AttributeTrail> attrs) {
  std::vector<cel::Handle<cel::Value>> result;
  result.reserve(args.size());
  for (size_t i = 0; i < args.size(); i++) {
    auto attr_set = frame->attribute_utility().CheckForUnknowns(
        attrs.subspan(i, 1), /*use_partial=*/true);
    if (!attr_set.empty()) {
      auto unknown_set = frame->memory_manager()
                             .New<UnknownSet>(std::move(attr_set))
                             .release();
      result.push_back(
          cel::interop_internal::CreateUnknownValueFromView(unknown_set));
    } else {
      result.push_back(args.at(i));
    }
  }

  return result;
}

bool IsUnknownFunctionResultError(const Handle<Value>& result) {
  if (!result->Is<cel::ErrorValue>()) {
    return false;
  }

  const auto& status = result.As<cel::ErrorValue>()->value();

  if (status.code() != absl::StatusCode::kUnavailable) {
    return false;
  }
  auto payload = status.GetPayload(
      cel::interop_internal::kPayloadUrlUnknownFunctionResult);
  return payload.has_value() && payload.value() == "true";
}

// Simple wrapper around a function resolution result. A function call should
// resolve to a single function implementation and a descriptor or none.
struct ResolveResult {
  static ResolveResult NoOverload() { return ResolveResult{}; }

  bool IsPresent() const { return descriptor != nullptr; }

  const cel::FunctionDescriptor* descriptor;
  const cel::Function* implementation;
};

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

  // Handles overload resolution and updating result appropriately.
  // Shouldn't update frame state.
  //
  // A non-ok result is an unrecoverable error, either from an illegal
  // evaluation state or forwarded from an extension function. Errors where
  // evaluation can reasonably condition are returned in the result as a
  // cel::ErrorValue.
  absl::StatusOr<Handle<Value>> DoEvaluate(ExecutionFrame* frame) const;

  virtual absl::StatusOr<ResolveResult> ResolveFunction(
      absl::Span<const cel::Handle<cel::Value>> args,
      const ExecutionFrame* frame) const = 0;

 protected:
  std::string name_;
  size_t num_arguments_;
};

absl::StatusOr<Handle<Value>> AbstractFunctionStep::DoEvaluate(
    ExecutionFrame* frame) const {
  // Create Span object that contains input arguments to the function.
  auto input_args = frame->value_stack().GetSpan(num_arguments_);

  std::vector<cel::Handle<cel::Value>> unknowns_args;
  // Preprocess args. If an argument is partially unknown, convert it to an
  // unknown attribute set.
  if (frame->enable_unknowns()) {
    auto input_attrs = frame->value_stack().GetAttributeSpan(num_arguments_);
    unknowns_args = CheckForPartialUnknowns(frame, input_args, input_attrs);
    input_args = absl::MakeConstSpan(unknowns_args);
  }

  // Derived class resolves to a single function overload or none.
  CEL_ASSIGN_OR_RETURN(ResolveResult matched_function,
                       ResolveFunction(input_args, frame));

  // Overload found and is allowed to consume the arguments.
  if (matched_function.IsPresent() &&
      ShouldAcceptOverload(*matched_function.descriptor, input_args)) {
    FunctionEvaluationContext context(frame->value_factory());

    CEL_ASSIGN_OR_RETURN(
        Handle<Value> result,
        matched_function.implementation->Invoke(context, input_args));

    if (frame->enable_unknown_function_results() &&
        IsUnknownFunctionResultError(result)) {
      auto unknown_set = frame->attribute_utility().CreateUnknownSet(
          *matched_function.descriptor, id(), input_args);
      return cel::interop_internal::CreateUnknownValueFromView(unknown_set);
    }
    return result;
  }

  // No matching overloads.
  // Such absence can be caused by presence of CelError in arguments.
  // To enable behavior of functions that accept CelError( &&, || ), CelErrors
  // should be propagated along execution path.
  for (const auto& arg : input_args) {
    if (arg->Is<cel::ErrorValue>()) {
      return arg;
    }
  }

  if (frame->enable_unknowns()) {
    // Already converted partial unknowns to unknown sets so just merge.
    auto unknown_set =
        frame->attribute_utility().MergeUnknowns(input_args, nullptr);
    if (unknown_set != nullptr) {
      return cel::interop_internal::CreateUnknownValueFromView(unknown_set);
    }
  }

  std::string arg_types;
  for (const auto& arg : input_args) {
    if (!arg_types.empty()) {
      absl::StrAppend(&arg_types, ", ");
    }
    absl::StrAppend(&arg_types, CelValue::TypeName(arg->kind()));
  }

  // If no errors or unknowns in input args, create new CelError for missing
  // overlaod.
  return cel::interop_internal::CreateErrorValueFromView(
      cel::interop_internal::CreateNoMatchingOverloadError(
          frame->memory_manager(), absl::StrCat(name_, "(", arg_types, ")")));
}

absl::Status AbstractFunctionStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(num_arguments_)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  // DoEvaluate may return a status for non-recoverable errors  (e.g.
  // unexpected typing, illegal expression state). Application errors that can
  // reasonably be handled as a cel error will appear in the result value.
  CEL_ASSIGN_OR_RETURN(auto result, DoEvaluate(frame));

  frame->value_stack().Pop(num_arguments_);
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

class EagerFunctionStep : public AbstractFunctionStep {
 public:
  EagerFunctionStep(std::vector<CelFunctionRegistry::StaticOverload> overloads,
                    const std::string& name, size_t num_args, int64_t expr_id)
      : AbstractFunctionStep(name, num_args, expr_id),
        overloads_(std::move(overloads)) {}

  absl::StatusOr<ResolveResult> ResolveFunction(
      absl::Span<const cel::Handle<cel::Value>> input_args,
      const ExecutionFrame* frame) const override;

 private:
  std::vector<CelFunctionRegistry::StaticOverload> overloads_;
};

absl::StatusOr<ResolveResult> EagerFunctionStep::ResolveFunction(
    absl::Span<const cel::Handle<cel::Value>> input_args,
    const ExecutionFrame* frame) const {
  ResolveResult result = ResolveResult::NoOverload();

  for (const auto& overload : overloads_) {
    if (ArgumentKindsMatch(*overload.descriptor, input_args)) {
      // More than one overload matches our arguments.
      if (result.IsPresent()) {
        return absl::Status(absl::StatusCode::kInternal,
                            "Cannot resolve overloads");
      }

      result = ResolveResult{overload.descriptor, overload.implementation};
    }
  }
  return result;
}

class LazyFunctionStep : public AbstractFunctionStep {
 public:
  // Constructs LazyFunctionStep that attempts to lookup function implementation
  // at runtime.
  LazyFunctionStep(const std::string& name, size_t num_args,
                   bool receiver_style,
                   std::vector<CelFunctionRegistry::LazyOverload> providers,
                   int64_t expr_id)
      : AbstractFunctionStep(name, num_args, expr_id),
        receiver_style_(receiver_style),
        providers_(std::move(providers)) {}

  absl::StatusOr<ResolveResult> ResolveFunction(
      absl::Span<const cel::Handle<cel::Value>> input_args,
      const ExecutionFrame* frame) const override;

 private:
  bool receiver_style_;
  std::vector<CelFunctionRegistry::LazyOverload> providers_;
};

absl::StatusOr<ResolveResult> LazyFunctionStep::ResolveFunction(
    absl::Span<const cel::Handle<cel::Value>> input_args,
    const ExecutionFrame* frame) const {
  ResolveResult result = ResolveResult::NoOverload();

  std::vector<CelValue::Type> arg_types(num_arguments_);

  std::transform(
      input_args.begin(), input_args.end(), arg_types.begin(),
      [](const cel::Handle<cel::Value>& value) { return value->kind(); });

  CelFunctionDescriptor matcher{name_, receiver_style_, arg_types};

  const BaseActivation& activation = frame->activation();
  for (auto provider : providers_) {
    // The LazyFunctionStep has so far only resolved by function shape, check
    // that the runtime argument kinds agree with the specific descriptor for
    // the provider candidates.
    if (!ArgumentKindsMatch(*provider.descriptor, input_args)) {
      continue;
    }

    CEL_ASSIGN_OR_RETURN(auto overload,
                         provider.provider->GetFunction(matcher, activation));

    if (overload != nullptr && overload->MatchArguments(input_args)) {
      const cel::Function* impl = overload;
      // More than one overload matches our arguments.
      if (result.IsPresent()) {
        return absl::Status(absl::StatusCode::kInternal,
                            "Cannot resolve overloads");
      }

      result = ResolveResult{&overload->descriptor(), impl};
    }
  }

  return result;
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::ast::internal::Call& call_expr, int64_t expr_id,
    std::vector<CelFunctionRegistry::LazyOverload> lazy_overloads) {
  bool receiver_style = call_expr.has_target();
  size_t num_args = call_expr.args().size() + (receiver_style ? 1 : 0);
  const std::string& name = call_expr.function();
  return std::make_unique<LazyFunctionStep>(name, num_args, receiver_style,
                                            std::move(lazy_overloads), expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::ast::internal::Call& call_expr, int64_t expr_id,
    std::vector<CelFunctionRegistry::StaticOverload> overloads) {
  bool receiver_style = call_expr.has_target();
  size_t num_args = call_expr.args().size() + (receiver_style ? 1 : 0);
  const std::string& name = call_expr.function();
  return std::make_unique<EagerFunctionStep>(std::move(overloads), name,
                                             num_args, expr_id);
}

}  // namespace google::api::expr::runtime
