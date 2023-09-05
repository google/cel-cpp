#include "eval/eval/evaluator_core.h"

#include <cstddef>
#include <memory>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "internal/status_macros.h"
#include "runtime/activation_interface.h"

namespace google::api::expr::runtime {

FlatExpressionEvaluatorState::FlatExpressionEvaluatorState(
    size_t value_stack_size, size_t comprehension_slot_count,
    const cel::TypeProvider& type_provider, cel::MemoryManager& memory_manager)
    : value_stack_(value_stack_size),
      comprehension_slots_(comprehension_slot_count),
      type_factory_(memory_manager),
      type_manager_(type_factory_, type_provider),
      value_factory_(type_manager_) {}

void FlatExpressionEvaluatorState::Reset() {
  value_stack_.Clear();
  comprehension_slots_.Reset();
}

const ExpressionStep* ExecutionFrame::Next() {
  size_t end_pos = execution_path_.size();

  if (pc_ < end_pos) return execution_path_[pc_++].get();
  if (pc_ > end_pos) {
    ABSL_LOG(ERROR) << "Attempting to step beyond the end of execution path.";
  }
  return nullptr;
}

absl::StatusOr<cel::Handle<cel::Value>> ExecutionFrame::Evaluate(
    const EvaluationListener& listener) {
  size_t initial_stack_size = value_stack().size();
  const ExpressionStep* expr;

  while ((expr = Next()) != nullptr) {
    CEL_RETURN_IF_ERROR(expr->Evaluate(this));

    if (!listener ||
        // This step was added during compilation (e.g. Int64ConstImpl).
        !expr->ComesFromAst()) {
      continue;
    }

    if (value_stack().empty()) {
      ABSL_LOG(ERROR) << "Stack is empty after a ExpressionStep.Evaluate. "
                         "Try to disable short-circuiting.";
      continue;
    }
    CEL_RETURN_IF_ERROR(
        listener(expr->id(), value_stack().Peek(), value_factory()));
  }

  size_t final_stack_size = value_stack().size();
  if (final_stack_size != initial_stack_size + 1 || final_stack_size == 0) {
    return absl::Status(absl::StatusCode::kInternal,
                        "Stack error during evaluation");
  }
  cel::Handle<cel::Value> value = value_stack().Peek();
  value_stack().Pop(1);
  return value;
}

FlatExpressionEvaluatorState FlatExpression::MakeEvaluatorState(
    cel::MemoryManager& manager) const {
  return FlatExpressionEvaluatorState(path_.size(), comprehension_slots_size_,
                                      type_provider_, manager);
}

absl::StatusOr<cel::Handle<cel::Value>> FlatExpression::EvaluateWithCallback(
    const cel::ActivationInterface& activation,
    const EvaluationListener& listener,
    FlatExpressionEvaluatorState& state) const {
  state.Reset();

  ExecutionFrame frame(path_, activation, options_, state);

  return frame.Evaluate(listener);
}

}  // namespace google::api::expr::runtime
