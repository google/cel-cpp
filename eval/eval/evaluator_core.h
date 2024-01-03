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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "common/native_type.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/evaluator_stack.h"
#include "runtime/activation_interface.h"
#include "runtime/managed_value_factory.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// Forward declaration of ExecutionFrame, to resolve circular dependency.
class ExecutionFrame;

using EvaluationListener = cel::TraceableProgram::EvaluationListener;

// Class Expression represents single execution step.
class ExpressionStep {
 public:
  virtual ~ExpressionStep() = default;

  // Performs actual evaluation.
  // Values are passed between Expression objects via EvaluatorStack, which is
  // supplied with context.
  // Also, Expression gets values supplied by caller though Activation
  // interface.
  // ExpressionStep instances can in specific cases
  // modify execution order(perform jumps).
  virtual absl::Status Evaluate(ExecutionFrame* context) const = 0;

  // Returns corresponding expression object ID.
  // Requires that the input expression has IDs assigned to sub-expressions,
  // e.g. via a checker. The default value 0 is returned if there is no
  // expression associated (e.g. a jump step), or if there is no ID assigned to
  // the corresponding expression. Useful for error scenarios where information
  // from Expr object is needed to create CelError.
  virtual int64_t id() const = 0;

  // Returns if the execution step comes from AST.
  virtual bool ComesFromAst() const = 0;

  // Returns a tight upper bound for stack changes required for this expression.
  // This is used to compute a reasonable size for the value stack to
  // pre-allocate.
  virtual int stack_delta() const = 0;

  // Return the type of the underlying expression step for special handling in
  // the planning phase. This should only be overridden by special cases, and
  // callers must not make any assumptions about the default case.
  virtual cel::NativeTypeId GetNativeTypeId() const = 0;
};

using ExecutionPath = std::vector<std::unique_ptr<const ExpressionStep>>;
using ExecutionPathView =
    absl::Span<const std::unique_ptr<const ExpressionStep>>;

// Class that wraps the state that needs to be allocated for expression
// evaluation. This can be reused to save on allocations.
class FlatExpressionEvaluatorState {
 public:
  FlatExpressionEvaluatorState(size_t value_stack_size,
                               size_t comprehension_slot_count,
                               const cel::TypeProvider& type_provider,
                               cel::MemoryManagerRef memory_manager);

  FlatExpressionEvaluatorState(size_t value_stack_size,
                               size_t comprehension_slot_count,
                               cel::ValueFactory& value_factory);

  void Reset();

  EvaluatorStack& value_stack() { return value_stack_; }

  ComprehensionSlots& comprehension_slots() { return comprehension_slots_; }

  cel::MemoryManagerRef memory_manager() {
    return value_factory_->memory_manager();
  }

  cel::TypeFactory& type_factory() { return value_factory_->type_factory(); }

  cel::TypeManager& type_manager() { return value_factory_->type_manager(); }

  cel::ValueFactory& value_factory() { return *value_factory_; }

 private:
  EvaluatorStack value_stack_;
  ComprehensionSlots comprehension_slots_;
  absl::optional<cel::ManagedValueFactory> managed_value_factory_;
  cel::ValueFactory* value_factory_;
};

// ExecutionFrame manages the context needed for expression evaluation.
// The lifecycle of the object is bound to a FlateExpression::Evaluate*(...)
// call.
class ExecutionFrame {
 public:
  // flat is the flattened sequence of execution steps that will be evaluated.
  // activation provides bindings between parameter names and values.
  // state contains the value factory for evaluation and the allocated data
  //   structures needed for evaluation.
  ExecutionFrame(ExecutionPathView flat,
                 const cel::ActivationInterface& activation,
                 const cel::RuntimeOptions& options,
                 FlatExpressionEvaluatorState& state)
      : pc_(0UL),
        execution_path_(flat),
        activation_(activation),
        options_(options),
        state_(state),
        attribute_utility_(activation_.GetUnknownAttributes(),
                           activation_.GetMissingAttributes(),
                           state_.value_factory()),
        max_iterations_(options_.comprehension_max_iterations),
        iterations_(0),
        subexpressions_() {}

  ExecutionFrame(absl::Span<const ExecutionPathView> subexpressions,
                 const cel::ActivationInterface& activation,
                 const cel::RuntimeOptions& options,
                 FlatExpressionEvaluatorState& state)
      : pc_(0UL),
        execution_path_(subexpressions[0]),
        activation_(activation),
        options_(options),
        state_(state),
        attribute_utility_(activation_.GetUnknownAttributes(),
                           activation_.GetMissingAttributes(),
                           state_.value_factory()),
        max_iterations_(options_.comprehension_max_iterations),
        iterations_(0),
        subexpressions_(subexpressions) {
    ABSL_DCHECK(!subexpressions.empty());
  }

  // Returns next expression to evaluate.
  const ExpressionStep* Next();

  // Evaluate the execution frame to completion.
  absl::StatusOr<cel::Handle<cel::Value>> Evaluate(EvaluationListener listener);

  // Intended for use in builtin shortcutting operations.
  //
  // Offset applies after normal pc increment. For example, JumpTo(0) is a
  // no-op, JumpTo(1) skips the expected next step.
  absl::Status JumpTo(int offset) {
    int new_pc = static_cast<int>(pc_) + offset;
    if (new_pc < 0 || new_pc > static_cast<int>(execution_path_.size())) {
      return absl::Status(absl::StatusCode::kInternal,
                          absl::StrCat("Jump address out of range: position: ",
                                       pc_, ",offset: ", offset,
                                       ", range: ", execution_path_.size()));
    }
    pc_ = static_cast<size_t>(new_pc);
    return absl::OkStatus();
  }

  // Move pc to a subexpression.
  //
  // Unlike a `Call` in a programming language, the subexpression is evaluated
  // in the same context as the caller (e.g. no stack isolation or scope change)
  //
  // Only intended for use in built-in notion of lazily evaluated
  // subexpressions.
  void Call(int return_pc_offset, size_t subexpression_index) {
    ABSL_DCHECK_LT(subexpression_index, subexpressions_.size());
    ExecutionPathView subexpression = subexpressions_[subexpression_index];
    ABSL_DCHECK(subexpression != execution_path_);
    int return_pc = static_cast<int>(pc_) + return_pc_offset;
    // return pc == size() is supported (a tail call).
    ABSL_DCHECK_GE(return_pc, 0);
    ABSL_DCHECK_LE(return_pc, static_cast<int>(execution_path_.size()));
    call_stack_.push_back(SubFrame{static_cast<size_t>(return_pc),
                                   value_stack().size() + 1, execution_path_});
    pc_ = 0UL;
    execution_path_ = subexpression;
  }

  EvaluatorStack& value_stack() { return state_.value_stack(); }
  ComprehensionSlots& comprehension_slots() {
    return state_.comprehension_slots();
  }

  bool enable_attribute_tracking() const {
    return options_.unknown_processing !=
               cel::UnknownProcessingOptions::kDisabled ||
           options_.enable_missing_attribute_errors;
  }

  bool enable_unknowns() const {
    return options_.unknown_processing !=
           cel::UnknownProcessingOptions::kDisabled;
  }

  bool enable_unknown_function_results() const {
    return options_.unknown_processing ==
           cel::UnknownProcessingOptions::kAttributeAndFunction;
  }

  bool enable_missing_attribute_errors() const {
    return options_.enable_missing_attribute_errors;
  }

  bool enable_heterogeneous_numeric_lookups() const {
    return options_.enable_heterogeneous_equality;
  }

  bool enable_comprehension_list_append() const {
    return options_.enable_comprehension_list_append;
  }

  cel::MemoryManagerRef memory_manager() { return state_.memory_manager(); }

  cel::TypeFactory& type_factory() { return state_.type_factory(); }

  cel::TypeManager& type_manager() { return state_.type_manager(); }

  cel::ValueFactory& value_factory() { return state_.value_factory(); }

  const AttributeUtility& attribute_utility() const {
    return attribute_utility_;
  }

  // Returns reference to the modern API activation.
  const cel::ActivationInterface& modern_activation() const {
    return activation_;
  }

  // Increment iterations and return an error if the iteration budget is
  // exceeded
  absl::Status IncrementIterations() {
    if (max_iterations_ == 0) {
      return absl::OkStatus();
    }
    iterations_++;
    if (iterations_ >= max_iterations_) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Iteration budget exceeded");
    }
    return absl::OkStatus();
  }

 private:
  struct SubFrame {
    size_t return_pc;
    size_t expected_stack_size;
    ExecutionPathView return_expression;
  };

  size_t pc_;  // pc_ - Program Counter. Current position on execution path.
  ExecutionPathView execution_path_;
  const cel::ActivationInterface& activation_;
  const cel::RuntimeOptions& options_;  // owned by the FlatExpr instance
  FlatExpressionEvaluatorState& state_;
  AttributeUtility attribute_utility_;
  const int max_iterations_;
  int iterations_;
  absl::Span<const ExecutionPathView> subexpressions_;
  std::vector<SubFrame> call_stack_;
};

// A flattened representation of the input CEL AST.
class FlatExpression {
 public:
  // path is flat execution path that is based upon the flattened AST tree
  // type_provider is the configured type system that should be used for
  //   value creation in evaluation
  FlatExpression(ExecutionPath path, size_t value_stack_size,
                 size_t comprehension_slots_size,
                 const cel::TypeProvider& type_provider,
                 const cel::RuntimeOptions& options)
      : path_(std::move(path)),
        subexpressions_({path_}),
        value_stack_size_(value_stack_size),
        comprehension_slots_size_(comprehension_slots_size),
        type_provider_(type_provider),
        options_(options) {}

  FlatExpression(ExecutionPath path,
                 std::vector<ExecutionPathView> subexpressions,
                 size_t value_stack_size, size_t comprehension_slots_size,
                 const cel::TypeProvider& type_provider,
                 const cel::RuntimeOptions& options)
      : path_(std::move(path)),
        subexpressions_(std::move(subexpressions)),
        value_stack_size_(value_stack_size),
        comprehension_slots_size_(comprehension_slots_size),
        type_provider_(type_provider),
        options_(options) {}

  // Move-only
  FlatExpression(FlatExpression&&) = default;
  FlatExpression& operator=(FlatExpression&&) = delete;

  // Create new evaluator state instance with the configured options and type
  // provider.
  FlatExpressionEvaluatorState MakeEvaluatorState(
      cel::MemoryManagerRef memory_manager) const;
  FlatExpressionEvaluatorState MakeEvaluatorState(
      cel::ValueFactory& value_factory) const;

  // Evaluate the expression.
  //
  // A status may be returned if an unexpected error occurs. Recoverable errors
  // will be represented as a cel::ErrorValue result.
  //
  // If the listener is not empty, it will be called after each evaluation step
  // that correlates to an AST node. The value passed to the will be the top of
  // the evaluation stack, corresponding to the result of the subexpression.
  absl::StatusOr<cel::Handle<cel::Value>> EvaluateWithCallback(
      const cel::ActivationInterface& activation, EvaluationListener listener,
      FlatExpressionEvaluatorState& state) const;

  const ExecutionPath& path() const { return path_; }

 private:
  ExecutionPath path_;
  std::vector<ExecutionPathView> subexpressions_;
  size_t value_stack_size_;
  size_t comprehension_slots_size_;
  const cel::TypeProvider& type_provider_;
  cel::RuntimeOptions options_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
