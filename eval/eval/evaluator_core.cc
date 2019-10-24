#include "eval/eval/evaluator_core.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::api::expr::v1alpha1::Expr;

const ExpressionStep* ExecutionFrame::Next() {
  size_t end_pos = execution_path_->size();

  if (pc_ < end_pos) return (*execution_path_)[pc_++].get();
  if (pc_ > end_pos) {
    GOOGLE_LOG(ERROR) << "Attempting to step beyond the end of execution path.";
  }
  return nullptr;
}

cel_base::StatusOr<CelValue> CelExpressionFlatImpl::Evaluate(
    const Activation& activation, google::protobuf::Arena* arena) const {
  return Trace(activation, arena, CelEvaluationListener());
}

cel_base::StatusOr<CelValue> CelExpressionFlatImpl::Trace(
    const Activation& activation, google::protobuf::Arena* arena,
    CelEvaluationListener callback) const {
  ExecutionFrame frame(&path_, activation, arena, max_iterations_);

  ValueStack* stack = &frame.value_stack();
  size_t initial_stack_size = stack->size();
  const ExpressionStep* expr;
  while ((expr = frame.Next()) != nullptr) {
    auto status = expr->Evaluate(&frame);
    if (!status.ok()) {
      return status;
    }
    if (!callback) {
      continue;
    }
    if (!expr->ComesFromAst()) {
      // This step was added during compilation (e.g. Int64ConstImpl).
      continue;
    }

    if (stack->size() == 0) {
      GOOGLE_LOG(ERROR) << "Stack is empty after a ExpressionStep.Evaluate. "
                    "Try to disable short-circuiting.";
      continue;
    }
    auto status2 = callback(expr->id(), stack->Peek(), arena);
    if (!status2.ok()) {
      return status2;
    }
  }

  size_t final_stack_size = stack->size();
  if (initial_stack_size + 1 != final_stack_size || final_stack_size == 0) {
    return cel_base::Status(cel_base::StatusCode::kInternal,
                        "Stack error during evaluation");
  }
  CelValue value = stack->Peek();
  stack->Pop(1);
  return value;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
