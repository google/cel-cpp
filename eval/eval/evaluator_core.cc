#include "eval/eval/evaluator_core.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/memory.h"
#include "base/type_provider.h"
#include "base/value_factory.h"
#include "eval/eval/attribute_trail.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

absl::Status InvalidIterationStateError() {
  return absl::InternalError(
      "Attempted to access iteration variable outside of comprehension.");
}

}  // namespace

FlatExpressionEvaluatorState::FlatExpressionEvaluatorState(
    size_t value_stack_size, const cel::TypeProvider& type_provider,
    cel::MemoryManager& memory_manager)
    : value_stack_(value_stack_size),
      type_factory_(memory_manager),
      type_manager_(type_factory_, type_provider),
      value_factory_(type_manager_) {}

void FlatExpressionEvaluatorState::Reset() {
  value_stack_.Clear();
  iter_stack_.clear();
}

const ExpressionStep* ExecutionFrame::Next() {
  size_t end_pos = execution_path_.size();

  if (pc_ < end_pos) return execution_path_[pc_++].get();
  if (pc_ > end_pos) {
    ABSL_LOG(ERROR) << "Attempting to step beyond the end of execution path.";
  }
  return nullptr;
}

absl::Status ExecutionFrame::PushIterFrame(absl::string_view iter_var_name,
                                           absl::string_view accu_var_name) {
  FlatExpressionEvaluatorState::IterFrame frame;
  frame.iter_var = {iter_var_name, cel::Handle<cel::Value>(), AttributeTrail()};
  frame.accu_var = {accu_var_name, cel::Handle<cel::Value>(), AttributeTrail()};
  state_.iter_stack().push_back(std::move(frame));
  return absl::OkStatus();
}

absl::Status ExecutionFrame::PopIterFrame() {
  if (state_.iter_stack().empty()) {
    return absl::InternalError("Loop stack underflow.");
  }
  state_.iter_stack().pop_back();
  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetAccuVar(cel::Handle<cel::Value> value) {
  return SetAccuVar(std::move(value), AttributeTrail());
}

absl::Status ExecutionFrame::SetAccuVar(cel::Handle<cel::Value> value,
                                        AttributeTrail trail) {
  if (state_.iter_stack().empty()) {
    return InvalidIterationStateError();
  }
  auto& iter = state_.IterStackTop();
  iter.accu_var.value = std::move(value);
  iter.accu_var.attr_trail = std::move(trail);
  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetIterVar(cel::Handle<cel::Value> value,
                                        AttributeTrail trail) {
  if (state_.iter_stack().empty()) {
    return InvalidIterationStateError();
  }
  auto& iter = state_.IterStackTop();
  iter.iter_var.value = std::move(value);
  iter.iter_var.attr_trail = std::move(trail);
  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetIterVar(cel::Handle<cel::Value> value) {
  return SetIterVar(std::move(value), AttributeTrail());
}

absl::Status ExecutionFrame::ClearIterVar() {
  if (state_.iter_stack().empty()) {
    return InvalidIterationStateError();
  }
  state_.IterStackTop().iter_var.value = cel::Handle<cel::Value>();
  return absl::OkStatus();
}

bool ExecutionFrame::GetIterVar(absl::string_view name,
                                cel::Handle<cel::Value>* value,
                                AttributeTrail* trail) const {
  for (auto iter = state_.iter_stack().rbegin();
       iter != state_.iter_stack().rend(); ++iter) {
    auto& frame = *iter;
    if (frame.iter_var.value && name == frame.iter_var.name) {
      if (value != nullptr) {
        *value = frame.iter_var.value;
      }
      if (trail != nullptr) {
        *trail = frame.iter_var.attr_trail;
      }
      return true;
    }
    if (frame.accu_var.value && name == frame.accu_var.name) {
      if (value != nullptr) {
        *value = frame.accu_var.value;
      }
      if (trail != nullptr) {
        *trail = frame.accu_var.attr_trail;
      }
      return true;
    }
  }

  return false;
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
  return FlatExpressionEvaluatorState(path_.size(), type_provider_, manager);
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
