#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_

#include <memory>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/types/optional.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/unknowns_utility.h"
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

// Class Expression represents single execution step.
class ExpressionStep {
 public:
  virtual ~ExpressionStep() {}

  // Performs actual evaluation.
  // Values are passed between Expression objects via ValueStack, which is
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

// CelValue stack.
// Implementation is based on vector to allow passing parameters from
// stack as Span<>.
class ValueStack {
 public:
  ValueStack(size_t max_size) : current_size_(0) {
    stack_.resize(max_size);
    attribute_stack_.resize(max_size);
  }

  // Return the current stack size.
  size_t size() const { return current_size_; }

  // Return the maximum size of the stack.
  size_t max_size() const { return stack_.size(); }

  // Returns true if stack is empty.
  bool empty() const { return current_size_ == 0; }

  // Attributes stack size.
  size_t attribute_size() const { return current_size_; }

  // Check that stack has enough elements.
  bool HasEnough(size_t size) const { return current_size_ >= size; }

  // Dumps the entire stack state as is.
  void Clear();

  // Gets the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  // Please note that calls to Push may invalidate returned Span object.
  absl::Span<const CelValue> GetSpan(size_t size) const {
    if (!HasEnough(size)) {
      GOOGLE_LOG(ERROR) << "Requested span size (" << size
                 << ") exceeds current stack size: " << current_size_;
    }
    return absl::Span<const CelValue>(stack_.data() + current_size_ - size,
                                      size);
  }

  // Gets the last size attribute trails of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  // Please note that calls to Push may invalidate returned Span object.
  absl::Span<const AttributeTrail> GetAttributeSpan(size_t size) const {
    return absl::Span<const AttributeTrail>(
        attribute_stack_.data() + current_size_ - size, size);
  }

  // Peeks the last element of the stack.
  // Checking that stack is not empty is caller's responsibility.
  const CelValue& Peek() const {
    if (empty()) {
      GOOGLE_LOG(ERROR) << "Peeking on empty ValueStack";
    }
    return stack_[current_size_ - 1];
  }

  // Peeks the last element of the attribute stack.
  // Checking that stack is not empty is caller's responsibility.
  const AttributeTrail& PeekAttribute() const {
    if (empty()) {
      GOOGLE_LOG(ERROR) << "Peeking on empty ValueStack";
    }
    return attribute_stack_[current_size_ - 1];
  }

  // Clears the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  void Pop(size_t size) {
    if (!HasEnough(size)) {
      GOOGLE_LOG(ERROR) << "Trying to pop more elements (" << size
                 << ") than the current stack size: " << current_size_;
    }
    current_size_ -= size;
  }

  // Put element on the top of the stack.
  void Push(const CelValue& value) { Push(value, AttributeTrail()); }

  void Push(const CelValue& value, AttributeTrail attribute) {
    if (current_size_ >= stack_.size()) {
      GOOGLE_LOG(ERROR) << "No room to push more elements on to ValueStack";
    }
    stack_[current_size_] = value;
    attribute_stack_[current_size_] = attribute;
    current_size_++;
  }

  // Replace element on the top of the stack.
  // Checking that stack is not empty is caller's responsibility.
  void PopAndPush(const CelValue& value) {
    PopAndPush(value, AttributeTrail());
  }

  // Replace element on the top of the stack.
  // Checking that stack is not empty is caller's responsibility.
  void PopAndPush(const CelValue& value, AttributeTrail attribute) {
    if (empty()) {
      GOOGLE_LOG(ERROR) << "Cannot PopAndPush on empty stack.";
    }
    stack_[current_size_ - 1] = value;
    attribute_stack_[current_size_ - 1] = attribute;
  }

  // Preallocate stack.
  void Reserve(size_t size) {
    stack_.reserve(size);
    attribute_stack_.reserve(size);
  }

 private:
  std::vector<CelValue> stack_;
  std::vector<AttributeTrail> attribute_stack_;
  size_t current_size_;
};

class CelExpressionFlatEvaluationState : public CelEvaluationState {
 public:
  CelExpressionFlatEvaluationState(
      size_t value_stack_size, const std::set<std::string>& iter_variable_names,
      google::protobuf::Arena* arena);

  void Reset();

  ValueStack& value_stack() { return value_stack_; }

  std::vector<std::map<std::string, absl::optional<CelValue>>>& iter_stack() {
    return iter_stack_;
  }

  std::map<std::string, absl::optional<CelValue>>& IterStackTop() {
    return iter_stack_[iter_stack().size() - 1];
  }

  std::set<std::string>& iter_variable_names() { return iter_variable_names_; }

  google::protobuf::Arena* arena() { return arena_; }

 private:
  ValueStack value_stack_;
  std::set<std::string> iter_variable_names_;
  std::vector<std::map<std::string, absl::optional<CelValue>>> iter_stack_;
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
                 bool enable_unknowns, bool enable_unknown_function_results)
      : pc_(0UL),
        execution_path_(flat),
        activation_(activation),
        enable_unknowns_(enable_unknowns),
        enable_unknown_function_results_(enable_unknown_function_results),
        unknowns_utility_(&activation.unknown_attribute_patterns(),
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

  ValueStack& value_stack() { return state_->value_stack(); }
  bool enable_unknowns() const { return enable_unknowns_; }
  bool enable_unknown_function_results() const {
    return enable_unknown_function_results_;
  }

  google::protobuf::Arena* arena() { return state_->arena(); }
  const UnknownsUtility& unknowns_utility() const { return unknowns_utility_; }

  // Returns reference to Activation
  const BaseActivation& activation() const { return activation_; }

  // Creates a new frame for iteration variables.
  absl::Status PushIterFrame();

  // Discards the top frame for iteration variables.
  absl::Status PopIterFrame();

  // Sets the value of an iteration variable
  absl::Status SetIterVar(const std::string& name, const CelValue& val);

  // Clears the value of an iteration variable
  absl::Status ClearIterVar(const std::string& name);

  // Gets the current value of an iteration variable.
  // Returns false if the variable is not currently in use (Set has been called
  // since init or last clear).
  bool GetIterVar(const std::string& name, CelValue* val) const;

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
  UnknownsUtility unknowns_utility_;
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
                        bool enable_unknown_function_results = false)
      : path_(std::move(path)),
        max_iterations_(max_iterations),
        iter_variable_names_(std::move(iter_variable_names)),
        enable_unknowns_(enable_unknowns),
        enable_unknown_function_results_(enable_unknown_function_results) {}

  // Move-only
  CelExpressionFlatImpl(const CelExpressionFlatImpl&) = delete;
  CelExpressionFlatImpl& operator=(const CelExpressionFlatImpl&) = delete;

  std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const override;

  // Implementation of CelExpression evaluate method.
  cel_base::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    google::protobuf::Arena* arena) const override {
    return Evaluate(activation, InitializeState(arena).get());
  }

  cel_base::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    CelEvaluationState* state) const override;

  // Implementation of CelExpression trace method.
  cel_base::StatusOr<CelValue> Trace(
      const BaseActivation& activation, google::protobuf::Arena* arena,
      CelEvaluationListener callback) const override {
    return Trace(activation, InitializeState(arena).get(), callback);
  }

  cel_base::StatusOr<CelValue> Trace(const BaseActivation& activation,
                                 CelEvaluationState* state,
                                 CelEvaluationListener callback) const override;

 private:
  const ExecutionPath path_;
  const int max_iterations_;
  const std::set<std::string> iter_variable_names_;
  bool enable_unknowns_;
  bool enable_unknown_function_results_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
