// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/eval/evaluator_core.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/utility/utility.h"
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
    const cel::TypeProvider& type_provider,
    cel::MemoryManagerRef memory_manager)
    : value_stack_(value_stack_size),
      comprehension_slots_(comprehension_slot_count),
      managed_value_factory_(absl::in_place, type_provider, memory_manager),
      value_factory_(&managed_value_factory_->get()) {}

FlatExpressionEvaluatorState::FlatExpressionEvaluatorState(
    size_t value_stack_size, size_t comprehension_slot_count,
    cel::ValueFactory& value_factory)
    : value_stack_(value_stack_size),
      comprehension_slots_(comprehension_slot_count),
      managed_value_factory_(absl::nullopt),
      value_factory_(&value_factory) {}

void FlatExpressionEvaluatorState::Reset() {
  value_stack_.Clear();
  comprehension_slots_.Reset();
}

const ExpressionStep* ExecutionFrame::Next() {
  size_t end_pos = execution_path_.size();

  if (pc_ < end_pos) return execution_path_[pc_++].get();
  if (pc_ == end_pos && !call_stack_.empty()) {
    pc_ = call_stack_.back().return_pc;
    execution_path_ = call_stack_.back().return_expression;
    ABSL_DCHECK_EQ(value_stack().size(),
                   call_stack_.back().expected_stack_size);
    call_stack_.pop_back();
    return Next();
  }
  if (pc_ > end_pos) {
    ABSL_LOG(ERROR) << "Attempting to step beyond the end of execution path.";
  }
  return nullptr;
}

absl::StatusOr<cel::Handle<cel::Value>> ExecutionFrame::Evaluate(
    EvaluationListener listener) {
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
    return absl::InternalError(absl::StrCat(
        "Stack error during evaluation: expected=", initial_stack_size + 1,
        ", actual=", final_stack_size));
  }
  cel::Handle<cel::Value> value = value_stack().Peek();
  value_stack().Pop(1);
  return value;
}

FlatExpressionEvaluatorState FlatExpression::MakeEvaluatorState(
    cel::MemoryManagerRef manager) const {
  return FlatExpressionEvaluatorState(
      value_stack_size_, comprehension_slots_size_, type_provider_, manager);
}

FlatExpressionEvaluatorState FlatExpression::MakeEvaluatorState(
    cel::ValueFactory& value_factory) const {
  return FlatExpressionEvaluatorState(value_stack_size_,
                                      comprehension_slots_size_, value_factory);
}

absl::StatusOr<cel::Handle<cel::Value>> FlatExpression::EvaluateWithCallback(
    const cel::ActivationInterface& activation, EvaluationListener listener,
    FlatExpressionEvaluatorState& state) const {
  state.Reset();

  ExecutionFrame frame(subexpressions_, activation, options_, state);

  return frame.Evaluate(std::move(listener));
}

}  // namespace google::api::expr::runtime
