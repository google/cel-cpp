#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/evaluator_stack.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Forward declaration of ExecutionFrame, to resolve circular dependency.
class ExecutionFrame;

using Expr = google::api::expr::v1alpha1::Expr;

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

  struct IterVarEntry {
    CelValue value;
    AttributeTrail attr_trail;
  };

  // Need pointer stability to avoid copying the attr trail lookups.
  using IterVarFrame = absl::node_hash_map<std::string, IterVarEntry>;

  void Reset();

  EvaluatorStack& value_stack() { return value_stack_; }

  std::vector<IterVarFrame>& iter_stack() { return iter_stack_; }

  IterVarFrame& IterStackTop() { return iter_stack_[iter_stack().size() - 1]; }

  std::set<std::string>& iter_variable_names() { return iter_variable_names_; }

  google::protobuf::Arena* arena() { return arena_; }

 private:
  EvaluatorStack value_stack_;
  std::set<std::string> iter_variable_names_;
  std::vector<IterVarFrame> iter_stack_;
  google::protobuf::Arena* arena_;
};

// ExecutionFrame provides context for expression evaluation.
// The lifecycle of the object is bound to CelExpression Evaluate(...) call.
class ExecutionFrame {
 public:
  // flat is the flattened sequence of execution steps that will be evaluated.
  // activation provides bindings between parameter names and values.
  // arena serves as allocation manager during the expression evaluation.

  ExecutionFrame(const ExecutionPath& flat, const BaseActivation& activation,
                 int max_iterations, CelExpressionFlatEvaluationState* state,
                 bool enable_unknowns, bool enable_unknown_function_results,
                 bool enable_missing_attribute_errors)
      : pc_(0UL),
        execution_path_(flat),
        activation_(activation),
        enable_unknowns_(enable_unknowns),
        enable_unknown_function_results_(enable_unknown_function_results),
        enable_missing_attribute_errors_(enable_missing_attribute_errors),
        attribute_utility_(&activation.unknown_attribute_patterns(),
                           &activation.missing_attribute_patterns(),
                           state->arena()),
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

  google::protobuf::Arena* arena() { return state_->arena(); }
  const AttributeUtility& attribute_utility() const {
    return attribute_utility_;
  }

  // Returns reference to Activation
  const BaseActivation& activation() const { return activation_; }

  // Creates a new frame for iteration variables.
  absl::Status PushIterFrame();

  // Discards the top frame for iteration variables.
  absl::Status PopIterFrame();

  // Sets the value of an iteration variable
  absl::Status SetIterVar(const std::string& name, const CelValue& val);

  // Sets the value of an iteration variable
  absl::Status SetIterVar(const std::string& name, const CelValue& val,
                          AttributeTrail trail);

  // Clears the value of an iteration variable
  absl::Status ClearIterVar(const std::string& name);

  // Gets the current value of an iteration variable.
  // Returns false if the variable is not currently in use (SetIterVar has been
  // called since init or last clear).
  bool GetIterVar(const std::string& name, CelValue* val) const;

  // Gets the current value of an iteration variable.
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
  bool enable_unknowns_;
  bool enable_unknown_function_results_;
  bool enable_missing_attribute_errors_;
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
  CelExpressionFlatImpl(const google::api::expr::v1alpha1::Expr* root_expr,
                        ExecutionPath path, int max_iterations,
                        std::set<std::string> iter_variable_names,
                        bool enable_unknowns = false,
                        bool enable_unknown_function_results = false,
                        bool enable_missing_attribute_errors = false,
                        std::unique_ptr<Expr> rewritten_expr = nullptr)
      : rewritten_expr_(std::move(rewritten_expr)),
        path_(std::move(path)),
        max_iterations_(max_iterations),
        iter_variable_names_(std::move(iter_variable_names)),
        enable_unknowns_(enable_unknowns),
        enable_unknown_function_results_(enable_unknown_function_results),
        enable_missing_attribute_errors_(enable_missing_attribute_errors) {}

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
  std::unique_ptr<Expr> rewritten_expr_;
  const ExecutionPath path_;
  const int max_iterations_;
  const std::set<std::string> iter_variable_names_;
  bool enable_unknowns_;
  bool enable_unknown_function_results_;
  bool enable_missing_attribute_errors_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
