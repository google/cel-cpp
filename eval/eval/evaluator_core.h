#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/ast.h"
#include "base/memory_manager.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/evaluator_stack.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "extensions/protobuf/memory_manager.h"

namespace google::api::expr::runtime {

// Forward declaration of ExecutionFrame, to resolve circular dependency.
class ExecutionFrame;

using Expr = ::google::api::expr::v1alpha1::Expr;

// Class Expression represents single execution step.
class ExpressionStep {
 public:
  virtual ~ExpressionStep() {}

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
};

using ExecutionPath = std::vector<std::unique_ptr<const ExpressionStep>>;

class CelExpressionFlatEvaluationState : public CelEvaluationState {
 public:
  CelExpressionFlatEvaluationState(
      size_t value_stack_size, const std::set<std::string>& iter_variable_names,
      google::protobuf::Arena* arena);

  struct ComprehensionVarEntry {
    absl::string_view name;
    // present if we're in part of the loop context where this can be accessed.
    absl::optional<CelValue> value;
    AttributeTrail attr_trail;
  };

  struct IterFrame {
    ComprehensionVarEntry iter_var;
    ComprehensionVarEntry accu_var;
  };

  void Reset();

  EvaluatorStack& value_stack() { return value_stack_; }

  std::vector<IterFrame>& iter_stack() { return iter_stack_; }

  IterFrame& IterStackTop() { return iter_stack_[iter_stack().size() - 1]; }

  std::set<std::string>& iter_variable_names() { return iter_variable_names_; }

  google::protobuf::Arena* arena() { return memory_manager_.arena(); }

  cel::MemoryManager& memory_manager() { return memory_manager_; }

 private:
  EvaluatorStack value_stack_;
  std::set<std::string> iter_variable_names_;
  std::vector<IterFrame> iter_stack_;
  // TODO(issues/5): State owns a ProtoMemoryManager to adapt from the client
  // provided arena. In the future, clients will have to maintain the particular
  // manager they want to use for evaluation.
  cel::extensions::ProtoMemoryManager memory_manager_;
};

// ExecutionFrame provides context for expression evaluation.
// The lifecycle of the object is bound to CelExpression Evaluate(...) call.
class ExecutionFrame {
 public:
  // flat is the flattened sequence of execution steps that will be evaluated.
  // activation provides bindings between parameter names and values.
  // arena serves as allocation manager during the expression evaluation.

  ExecutionFrame(const ExecutionPath& flat, const BaseActivation& activation,
                 const CelTypeRegistry* type_registry, int max_iterations,
                 CelExpressionFlatEvaluationState* state, bool enable_unknowns,
                 bool enable_unknown_function_results,
                 bool enable_missing_attribute_errors,
                 bool enable_null_coercion,
                 bool enable_heterogeneous_numeric_lookups)
      : pc_(0UL),
        execution_path_(flat),
        activation_(activation),
        type_registry_(*type_registry),
        enable_unknowns_(enable_unknowns),
        enable_unknown_function_results_(enable_unknown_function_results),
        enable_missing_attribute_errors_(enable_missing_attribute_errors),
        enable_null_coercion_(enable_null_coercion),
        enable_heterogeneous_numeric_lookups_(
            enable_heterogeneous_numeric_lookups),
        attribute_utility_(&activation.unknown_attribute_patterns(),
                           &activation.missing_attribute_patterns(),
                           state->memory_manager()),
        max_iterations_(max_iterations),
        iterations_(0),
        state_(state) {}

  // Returns next expression to evaluate.
  const ExpressionStep* Next();

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

  EvaluatorStack& value_stack() { return state_->value_stack(); }
  bool enable_unknowns() const { return enable_unknowns_; }
  bool enable_unknown_function_results() const {
    return enable_unknown_function_results_;
  }
  bool enable_missing_attribute_errors() const {
    return enable_missing_attribute_errors_;
  }

  bool enable_null_coercion() const { return enable_null_coercion_; }

  bool enable_heterogeneous_numeric_lookups() const {
    return enable_heterogeneous_numeric_lookups_;
  }

  cel::MemoryManager& memory_manager() { return state_->memory_manager(); }

  const CelTypeRegistry& type_registry() { return type_registry_; }

  const AttributeUtility& attribute_utility() const {
    return attribute_utility_;
  }

  // Returns reference to Activation
  const BaseActivation& activation() const { return activation_; }

  // Creates a new frame for the iteration variables identified by iter_var_name
  // and accu_var_name.
  absl::Status PushIterFrame(absl::string_view iter_var_name,
                             absl::string_view accu_var_name);

  // Discards the top frame for iteration variables.
  absl::Status PopIterFrame();

  // Sets the value of the accumuation variable
  absl::Status SetAccuVar(const CelValue& val);

  // Sets the value of the accumulation variable
  absl::Status SetAccuVar(const CelValue& val, AttributeTrail trail);

  // Sets the value of the iteration variable
  absl::Status SetIterVar(const CelValue& val);

  // Sets the value of the iteration variable
  absl::Status SetIterVar(const CelValue& val, AttributeTrail trail);

  // Clears the value of the iteration variable
  absl::Status ClearIterVar();

  // Gets the current value of either an iteration variable or accumulation
  // variable.
  // Returns false if the variable is not yet set or has been cleared.
  bool GetIterVar(const std::string& name, CelValue* val) const;

  // Gets the current attribute trail of either an iteration variable or
  // accumulation variable.
  // Returns false if the variable is not currently in use (SetIterVar has not
  // been called since init or last clear).
  bool GetIterAttr(const std::string& name, const AttributeTrail** val) const;

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
  const ExecutionPath& execution_path_;
  const BaseActivation& activation_;
  const CelTypeRegistry& type_registry_;
  bool enable_unknowns_;
  bool enable_unknown_function_results_;
  bool enable_missing_attribute_errors_;
  bool enable_null_coercion_;
  bool enable_heterogeneous_numeric_lookups_;
  AttributeUtility attribute_utility_;
  const int max_iterations_;
  int iterations_;
  CelExpressionFlatEvaluationState* state_;
};

// Implementation of the CelExpression that utilizes flattening
// of the expression tree.
class CelExpressionFlatImpl : public CelExpression {
 public:
  // Constructs CelExpressionFlatImpl instance.
  // path is flat execution path that is based upon
  // flattened AST tree. Max iterations dictates the maximum number of
  // iterations in the comprehension expressions (use 0 to disable the upper
  // bound).
  // TODO(issues/5): Remove unused parameter \a root_expr.
  CelExpressionFlatImpl(
      ABSL_ATTRIBUTE_UNUSED const cel::ast::internal::Expr* root_expr,
      ExecutionPath path, const CelTypeRegistry* type_registry,
      int max_iterations, std::set<std::string> iter_variable_names,
      bool enable_unknowns = false,
      bool enable_unknown_function_results = false,
      bool enable_missing_attribute_errors = false,
      bool enable_null_coercion = true,
      bool enable_heterogeneous_equality = false,
      std::unique_ptr<cel::ast::internal::Expr> rewritten_expr = nullptr)
      : rewritten_expr_(std::move(rewritten_expr)),
        path_(std::move(path)),
        type_registry_(*type_registry),
        max_iterations_(max_iterations),
        iter_variable_names_(std::move(iter_variable_names)),
        enable_unknowns_(enable_unknowns),
        enable_unknown_function_results_(enable_unknown_function_results),
        enable_missing_attribute_errors_(enable_missing_attribute_errors),
        enable_null_coercion_(enable_null_coercion),
        enable_heterogeneous_equality_(enable_heterogeneous_equality) {}

  // Move-only
  CelExpressionFlatImpl(const CelExpressionFlatImpl&) = delete;
  CelExpressionFlatImpl& operator=(const CelExpressionFlatImpl&) = delete;

  std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const override;

  // Implementation of CelExpression evaluate method.
  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    google::protobuf::Arena* arena) const override {
    return Evaluate(activation, InitializeState(arena).get());
  }

  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    CelEvaluationState* state) const override;

  // Implementation of CelExpression trace method.
  absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, google::protobuf::Arena* arena,
      CelEvaluationListener callback) const override {
    return Trace(activation, InitializeState(arena).get(), callback);
  }

  absl::StatusOr<CelValue> Trace(const BaseActivation& activation,
                                 CelEvaluationState* state,
                                 CelEvaluationListener callback) const override;

 private:
  // Maintain lifecycle of a modified expression.
  std::unique_ptr<cel::ast::internal::Expr> rewritten_expr_;
  const ExecutionPath path_;
  const CelTypeRegistry& type_registry_;
  const int max_iterations_;
  const std::set<std::string> iter_variable_names_;
  bool enable_unknowns_;
  bool enable_unknown_function_results_;
  bool enable_missing_attribute_errors_;
  bool enable_null_coercion_;
  bool enable_heterogeneous_equality_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
