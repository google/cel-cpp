#include "eval/eval/evaluator_core.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "eval/eval/attribute_trail.h"
#include "eval/public/cel_value.h"
#include "internal/casts.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

absl::Status CheckIterAccess(CelExpressionFlatEvaluationState* state,
                             const std::string& name) {
  if (state->iter_stack().empty()) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrCat(
            "Attempted to update iteration variable outside of comprehension.'",
            name, "'"));
  }
  auto iter = state->iter_variable_names().find(name);
  if (iter == state->iter_variable_names().end()) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrCat("Attempted to set unknown variable '", name, "'"));
  }

  return absl::OkStatus();
}

}  // namespace

CelExpressionFlatEvaluationState::CelExpressionFlatEvaluationState(
    size_t value_stack_size, const std::set<std::string>& iter_variable_names,
    google::protobuf::Arena* arena)
    : value_stack_(value_stack_size),
      iter_variable_names_(iter_variable_names),
      arena_(arena) {}

void CelExpressionFlatEvaluationState::Reset() {
  iter_stack_.clear();
  value_stack_.Clear();
}

const ExpressionStep* ExecutionFrame::Next() {
  size_t end_pos = execution_path_.size();

  if (pc_ < end_pos) return execution_path_[pc_++].get();
  if (pc_ > end_pos) {
    GOOGLE_LOG(ERROR) << "Attempting to step beyond the end of execution path.";
  }
  return nullptr;
}

absl::Status ExecutionFrame::PushIterFrame() {
  state_->iter_stack().push_back({});
  return absl::OkStatus();
}

absl::Status ExecutionFrame::PopIterFrame() {
  if (state_->iter_stack().empty()) {
    return absl::InternalError("Loop stack underflow.");
  }
  state_->iter_stack().pop_back();
  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetIterVar(const std::string& name,
                                        const CelValue& val,
                                        AttributeTrail trail) {
  CEL_RETURN_IF_ERROR(CheckIterAccess(state_, name));
  state_->IterStackTop()[name] = {val, trail};

  return absl::OkStatus();
}

absl::Status ExecutionFrame::SetIterVar(const std::string& name,
                                        const CelValue& val) {
  return SetIterVar(name, val, AttributeTrail());
}

absl::Status ExecutionFrame::ClearIterVar(const std::string& name) {
  CEL_RETURN_IF_ERROR(CheckIterAccess(state_, name));
  state_->IterStackTop().erase(name);
  return absl::OkStatus();
}

bool ExecutionFrame::GetIterVar(const std::string& name, CelValue* val) const {
  absl::Status status = CheckIterAccess(state_, name);
  if (!status.ok()) {
    return false;
  }

  for (auto iter = state_->iter_stack().rbegin();
       iter != state_->iter_stack().rend(); ++iter) {
    auto& frame = *iter;
    auto frame_iter = frame.find(name);
    if (frame_iter != frame.end()) {
      const auto& entry = frame_iter->second;
      *val = entry.value;
      return true;
    }
  }

  return false;
}

bool ExecutionFrame::GetIterAttr(const std::string& name,
                                 const AttributeTrail** val) const {
  absl::Status status = CheckIterAccess(state_, name);
  if (!status.ok()) {
    return false;
  }

  for (auto iter = state_->iter_stack().rbegin();
       iter != state_->iter_stack().rend(); ++iter) {
    auto& frame = *iter;
    auto frame_iter = frame.find(name);
    if (frame_iter != frame.end()) {
      const auto& entry = frame_iter->second;
      *val = &entry.attr_trail;
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

  // Using both unknown attribute patterns and unknown paths via FieldMask is
  // not allowed.
  if (activation.unknown_paths().paths_size() != 0 &&
      !activation.unknown_attribute_patterns().empty()) {
    return absl::InvalidArgumentError(
        "Attempting to evaluate expression with both unknown_paths and "
        "unknown_attribute_patterns set in the Activation");
  }

  ExecutionFrame frame(path_, activation, max_iterations_, state,
                       enable_unknowns_, enable_unknown_function_results_,
                       enable_missing_attribute_errors_);

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
      GOOGLE_LOG(ERROR) << "Stack is empty after a ExpressionStep.Evaluate. "
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
