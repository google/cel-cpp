#include "eval/eval/function_step.h"
#include "eval/eval/expression_step_base.h"
#include "absl/strings/str_cat.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

// Implementation of ExpressionStep that finds suitable
// CelFunction overload and invokes it.
class FunctionStep : public ExpressionStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  FunctionStep(std::vector<const CelFunction*> overloads,
               const google::api::expr::v1alpha1::Expr* expr)
      : ExpressionStepBase(expr),
        overloads_(std::move(overloads)),
        num_arguments_(0) {
    if (!overloads_.empty()) {
      num_arguments_ = overloads_[0]->descriptor().types.size();
    }
  }

  util::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::vector<const CelFunction*> overloads_;
  int num_arguments_;
};

util::Status FunctionStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(num_arguments_)) {
    return util::MakeStatus(google::rpc::Code::INTERNAL, "Value stack underflow");
  }

  // Create Span object that contains input arguments to the function.
  auto input_args = frame->value_stack().GetSpan(num_arguments_);
  const CelFunction* matched_function = nullptr;

  for (auto overload : overloads_) {
    if (overload->MatchArguments(input_args)) {
      // More than one overload matches our arguments.
      if (matched_function != nullptr) {
        return util::MakeStatus(google::rpc::Code::INTERNAL,
                            "Cannot resolve overloads");
      }

      matched_function = overload;
    }
  }

  CelValue result = CelValue::CreateNull();

  // Overload found
  if (matched_function != nullptr) {
    util::Status status =
        matched_function->Evaluate(input_args, &result, frame->arena());
    if (!util::IsOk(status)) {
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
      result = CreateErrorValue(frame->arena(), "No matching overloads found",
                                CelError::Code::CelError_Code_UNKNOWN);
    }
  }

  frame->value_stack().Pop(num_arguments_);
  frame->value_stack().Push(result);

  return util::OkStatus();
}

}  // namespace

util::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const google::api::expr::v1alpha1::Expr::Call* call_expr,
    const google::api::expr::v1alpha1::Expr* expr,
    const CelFunctionRegistry& function_registry) {
  bool receiver_style = call_expr->has_target();

  std::vector<CelValue::Type> args(
      call_expr->args_size() + (receiver_style ? 1 : 0), CelValue::Type::kAny);

  auto overloads = function_registry.FindOverloads(call_expr->function(),
                                                   receiver_style, args);

  return CreateFunctionStep(expr, overloads);
}

util::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const google::api::expr::v1alpha1::Expr* expr,
    std::vector<const CelFunction*> overloads) {
  if (overloads.empty()) {
    return util::MakeStatus(google::rpc::Code::INVALID_ARGUMENT,
                          "No overloads provided for FunctionStep creation");
  }

  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<FunctionStep>(std::move(overloads), expr);

  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
