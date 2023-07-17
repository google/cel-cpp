#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/evaluator_stack.h"
#include "internal/rtti.h"
#include "runtime/activation_interface.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// Forward declaration of ExecutionFrame, to resolve circular dependency.
class ExecutionFrame;

using EvaluationListener = std::function<absl::Status(
    int64_t expr_id, const cel::Handle<cel::Value>&, cel::ValueFactory&)>;

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

  // Return the type of the underlying expression step for special handling in
  // the planning phase. This should only be overridden by special cases, and
  // callers must not make any assumptions about the default case.
  virtual cel::internal::TypeInfo TypeId() const = 0;
};

using ExecutionPath = std::vector<std::unique_ptr<const ExpressionStep>>;
using ExecutionPathView =
    absl::Span<const std::unique_ptr<const ExpressionStep>>;

// Class that wraps the state that needs to be allocated for expression
// evaluation. This can be reused to save on allocations.
class FlatExpressionEvaluatorState {
 public:
  struct ComprehensionVarEntry {
    absl::string_view name;
    // present if we're in part of the loop context where this can be accessed.
    cel::Handle<cel::Value> value;
    AttributeTrail attr_trail;
  };

  struct IterFrame {
    ComprehensionVarEntry iter_var;
    ComprehensionVarEntry accu_var;
  };

  FlatExpressionEvaluatorState(size_t value_stack_size,
                               const cel::TypeProvider& type_provider,
                               cel::MemoryManager& memory_manager);

  void Reset();

  EvaluatorStack& value_stack() { return value_stack_; }

  std::vector<IterFrame>& iter_stack() { return iter_stack_; }

  IterFrame& IterStackTop() { return iter_stack_[iter_stack().size() - 1]; }

  cel::MemoryManager& memory_manager() {
    return value_factory_.memory_manager();
  }

  cel::TypeFactory& type_factory() { return type_factory_; }

  cel::TypeManager& type_manager() { return type_manager_; }

  cel::ValueFactory& value_factory() { return value_factory_; }

 private:
  EvaluatorStack value_stack_;
  std::vector<IterFrame> iter_stack_;
  cel::TypeFactory type_factory_;
  cel::TypeManager type_manager_;
  cel::ValueFactory value_factory_;
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
        iterations_(0) {}

  // Returns next expression to evaluate.
  const ExpressionStep* Next();

  // Evaluate the execution frame to completion.
  absl::StatusOr<cel::Handle<cel::Value>> Evaluate(
      const EvaluationListener& listener);

  // Intended for use only in conditionals.
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

  EvaluatorStack& value_stack() { return state_.value_stack(); }

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

  cel::MemoryManager& memory_manager() { return state_.memory_manager(); }

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

  // Creates a new frame for the iteration variables identified by iter_var_name
  // and accu_var_name.
  absl::Status PushIterFrame(absl::string_view iter_var_name,
                             absl::string_view accu_var_name);

  // Discards the top frame for iteration variables.
  absl::Status PopIterFrame();

  // Sets the value of the accumulation variable
  absl::Status SetAccuVar(cel::Handle<cel::Value> value);

  // Sets the value of the accumulation variable
  absl::Status SetAccuVar(cel::Handle<cel::Value> value, AttributeTrail trail);

  // Sets the value of the iteration variable
  absl::Status SetIterVar(cel::Handle<cel::Value> value);

  // Sets the value of the iteration variable
  absl::Status SetIterVar(cel::Handle<cel::Value> value, AttributeTrail trail);

  // Clears the value of the iteration variable
  absl::Status ClearIterVar();

  // Gets the current value of either an iteration variable or accumulation
  // variable.
  // Returns false if the variable is not yet set or has been cleared.
  bool GetIterVar(absl::string_view name, cel::Handle<cel::Value>* value,
                  AttributeTrail* trail) const;

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
  size_t pc_;  // pc_ - Program Counter. Current position on execution path.
  ExecutionPathView execution_path_;
  const cel::ActivationInterface& activation_;
  const cel::RuntimeOptions& options_;  // owned by the FlatExpr instance
  FlatExpressionEvaluatorState& state_;
  AttributeUtility attribute_utility_;
  const int max_iterations_;
  int iterations_;
};

// A flattened representation of the input CEL AST.
class FlatExpression {
 public:
  // path is flat execution path that is based upon the flattened AST tree
  // type_provider is the configured type system that should be used for
  //   value creation in evaluation
  FlatExpression(ExecutionPath path, const cel::TypeProvider& type_provider,
                 const cel::RuntimeOptions& options)
      : path_(std::move(path)),
        type_provider_(type_provider),
        options_(options) {}

  // Move-only
  FlatExpression(FlatExpression&&) = default;
  FlatExpression& operator=(FlatExpression&&) = default;

  // Create new evaluator state instance with the configured options and type
  // provider.
  FlatExpressionEvaluatorState MakeEvaluatorState(
      cel::MemoryManager& memory_manager) const;

  // Evaluate the expression.
  //
  // A status may be returned if an unexpected error occurs. Recoverable errors
  // will be represented as a cel::ErrorValue result.
  //
  // If the listener is not empty, it will be called after each evaluation step
  // that correlates to an AST node. The value passed to the will be the top of
  // the evaluation stack, corresponding to the result of the subexpression.
  absl::StatusOr<cel::Handle<cel::Value>> EvaluateWithCallback(
      const cel::ActivationInterface& activation,
      const EvaluationListener& listener,
      FlatExpressionEvaluatorState& state) const;

  const ExecutionPath& path() const { return path_; }

 private:
  ExecutionPath path_;
  const cel::TypeProvider& type_provider_;
  cel::RuntimeOptions options_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
