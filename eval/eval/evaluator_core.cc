#include "eval/eval/evaluator_core.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "eval/eval/attribute_trail.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

absl::Status InvalidIterationStateError() {
  return absl::InternalError(
      "Attempted to access iteration variable outside of comprehension.");
}

}  // namespace

CelExpressionFlatEvaluationState::CelExpressionFlatEvaluationState(
    size_t value_stack_size, const std::set<std::string>& iter_variable_names,
    google::protobuf::Arena* arena)
    : value_stack_(value_stack_size),
      iter_variable_names_(iter_variable_names),
      memory_manager_(arena) {}

void CelExpressionFlatEvaluationState::Reset() {
  iter_stack_.clear();
  value_stack_.Clear();
}

const ExpressionStep* ExecutionFrame::Next() {
  size_t end_pos = execution_path_.size();

  if (pc_ < end_pos) return execution_path_[pc_++].get();
  if (pc_ > end_pos) {
    LOG(ERROR) << "Attempting to step beyond the end of execution path.";
  }
  return nullptr;
}

absl::Status ExecutionFrame::PushIterFrame(absl::string_view iter_var_name,
                                           absl::string_view accu_var_name) {
  CelExpressionFlatEvaluationState::IterFrame frame;
  frame.iter_var = {iter_var_name, absl::nullopt, AttributeTrail()};
  frame.accu_var = {accu_var_name, absl::nullopt, AttributeTrail()};
  state_->iter_stack().push_back(frame);
  return absl::OkStatus();
}

absl::Status ExecutionFrame::PopIterFrame() {
  if (state_->iter_stack().empty()) {
    return absl::InternalError("Loop stack underflow.");
  }
  state_->iter_stack().pop_back();
  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetAccuVar(const CelValue& val) {
  return SetAccuVar(val, AttributeTrail());
}

absl::Status ExecutionFrame::SetAccuVar(const CelValue& val,
                                        AttributeTrail trail) {
  if (state_->iter_stack().empty()) {
    return InvalidIterationStateError();
  }
  auto& iter = state_->IterStackTop();
  iter.accu_var.value = val;
  iter.accu_var.attr_trail = trail;
  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetIterVar(const CelValue& val,
                                        AttributeTrail trail) {
  if (state_->iter_stack().empty()) {
    return InvalidIterationStateError();
  }
  auto& iter = state_->IterStackTop();
  iter.iter_var.value = val;
  iter.iter_var.attr_trail = trail;
  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetIterVar(const CelValue& val) {
  return SetIterVar(val, AttributeTrail());
}

absl::Status ExecutionFrame::ClearIterVar() {
  if (state_->iter_stack().empty()) {
    return InvalidIterationStateError();
  }
  state_->IterStackTop().iter_var.value.reset();
  return absl::OkStatus();
}

bool ExecutionFrame::GetIterVar(const std::string& name, CelValue* val) const {
  for (auto iter = state_->iter_stack().rbegin();
       iter != state_->iter_stack().rend(); ++iter) {
    auto& frame = *iter;
    if (frame.iter_var.value.has_value() && name == frame.iter_var.name) {
      *val = *frame.iter_var.value;
      return true;
    }
    if (frame.accu_var.value.has_value() && name == frame.accu_var.name) {
      *val = *frame.accu_var.value;
      return true;
    }
  }

  return false;
}

bool ExecutionFrame::GetIterAttr(const std::string& name,
                                 const AttributeTrail** val) const {
  for (auto iter = state_->iter_stack().rbegin();
       iter != state_->iter_stack().rend(); ++iter) {
    auto& frame = *iter;
    if (frame.iter_var.value.has_value() && name == frame.iter_var.name) {
      *val = &frame.iter_var.attr_trail;
      return true;
    }
    if (frame.accu_var.value.has_value() && name == frame.accu_var.name) {
      *val = &frame.accu_var.attr_trail;
      return true;
    }
  }

  return false;
}

std::unique_ptr<CelEvaluationState> CelExpressionFlatImpl::InitializeState(
    google::protobuf::Arena* arena) const {
  return absl::make_unique<CelExpressionFlatEvaluationState>(
      path_.size(), iter_variable_names_, arena);
}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Evaluate(
    const BaseActivation& activation, CelEvaluationState* state) const {
  return Trace(activation, state, CelEvaluationListener());
}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Trace(
    const BaseActivation& activation, CelEvaluationState* _state,
    CelEvaluationListener callback) const {
  auto state =
      ::cel::internal::down_cast<CelExpressionFlatEvaluationState*>(_state);
  state->Reset();

  ExecutionFrame frame(path_, activation, &type_registry_, max_iterations_,
                       state, enable_unknowns_,
                       enable_unknown_function_results_,
                       enable_missing_attribute_errors_, enable_null_coercion_,
                       enable_heterogeneous_equality_);

  EvaluatorStack* stack = &frame.value_stack();
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

    if (stack->empty()) {
      LOG(ERROR) << "Stack is empty after a ExpressionStep.Evaluate. "
                    "Try to disable short-circuiting.";
      continue;
    }
    auto status2 = callback(expr->id(), stack->Peek(), state->arena());
    if (!status2.ok()) {
      return status2;
    }
  }

  size_t final_stack_size = stack->size();
  if (initial_stack_size + 1 != final_stack_size || final_stack_size == 0) {
    return absl::Status(absl::StatusCode::kInternal,
                        "Stack error during evaluation");
  }
  CelValue value = stack->Peek();
  stack->Pop(1);
  return value;
}

}  // namespace google::api::expr::runtime
