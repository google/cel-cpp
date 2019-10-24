#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_

#include "eval/public/activation.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

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
  virtual cel_base::Status Evaluate(ExecutionFrame* context) const = 0;

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

// CelValue stack.
// Implementation is based on vector to allow passing parameters from
// stack as Span<>.
using ExecutionPath = std::vector<std::unique_ptr<const ExpressionStep>>;

// CelValue stack.
// Implementation is based on vector to allow passing parameters from
// stack as Span<>.
class ValueStack {
 public:
  ValueStack() = default;

  // Stack size.
  int size() const { return stack_.size(); }

  // Check that stack has enough elements.
  bool HasEnough(int size) const { return stack_.size() >= size; }

  // Gets the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  // Please note that calls to Push may invalidate returned Span object.
  absl::Span<const CelValue> GetSpan(int size) const {
    return absl::Span<const CelValue>(stack_.data() + stack_.size() - size,
                                      size);
  }

  // Peeks the last element of the stack.
  // Checking that stack is not empty is caller's responsibility.
  const CelValue& Peek() const { return stack_.back(); }

  // Clears the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  void Pop(int size) { stack_.resize(stack_.size() - size); }

  // Put element on the top of the stack.
  void Push(const CelValue& value) { stack_.push_back(value); }

  // Replace element on the top of the stack.
  // Checking that stack is not empty is caller's responsibility.
  void PopAndPush(const CelValue& value) { stack_.back() = value; }

  // Preallocate stack.
  void Reserve(int size) { stack_.reserve(size); }

 private:
  std::vector<CelValue> stack_;
};

// ExecutionFrame provides context for expression evaluation.
// The lifecycle of the object is bound to CelExpression Evaluate(...) call.
class ExecutionFrame {
 public:
  // flat is the flattened sequence of execution steps that will be evaluated.
  // activation provides bindings between parameter names and values.
  // arena serves as allocation manager during the expression evaluation.
  ExecutionFrame(const ExecutionPath* flat, const Activation& activation,
                 google::protobuf::Arena* arena, int max_iterations)
      : pc_(0),
        execution_path_(flat),
        activation_(activation),
        arena_(arena),
        max_iterations_(max_iterations),
        iterations_(0) {
    // Reserve space on stack to minimize reallocations
    // on stack resize.
    value_stack_.Reserve(flat->size());
  }

  // Returns next expression to evaluate.
  const ExpressionStep* Next();

  // Intended for use only in conditionals.
  cel_base::Status JumpTo(int offset) {
    int new_pc = pc_ + offset;
    if (new_pc < 0 || new_pc > execution_path_->size()) {
      return cel_base::Status(cel_base::StatusCode::kInternal,
                          absl::StrCat("Jump address out of range: position: ",
                                       pc_, ",offset: ", offset,
                                       ", range: ", execution_path_->size()));
    }
    pc_ = new_pc;
    return cel_base::OkStatus();
  }

  ValueStack& value_stack() { return value_stack_; }

  google::protobuf::Arena* arena() { return arena_; }

  // Returns reference to Activation
  const Activation& activation() const { return activation_; }

  // Returns reference to iter_vars
  std::map<std::string, CelValue>& iter_vars() { return iter_vars_; }

  // Increment iterations and return an error if the iteration budget is
  // exceeded
  cel_base::Status IncrementIterations() {
    if (max_iterations_ == 0) {
      return cel_base::OkStatus();
    }
    iterations_++;
    if (iterations_ >= max_iterations_) {
      return cel_base::Status(cel_base::StatusCode::kInternal,
                          "Iteration budget exceeded");
    }
    return cel_base::OkStatus();
  }

 private:
  int pc_;  // pc_ - Program Counter. Current position on execution path.
  const ExecutionPath* execution_path_;
  const Activation& activation_;
  ValueStack value_stack_;
  google::protobuf::Arena* arena_;
  const int max_iterations_;
  int iterations_;
  std::map<std::string, CelValue> iter_vars_;  // variables declared in the frame.
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
  CelExpressionFlatImpl(const google::api::expr::v1alpha1::Expr*, ExecutionPath path,
                        int max_iterations)
      : path_(std::move(path)), max_iterations_(max_iterations) {}

  // Implementation of CelExpression evaluate method.
  cel_base::StatusOr<CelValue> Evaluate(const Activation& activation,
                                    google::protobuf::Arena* arena) const override;

  // Implementation of CelExpression trace method.
  cel_base::StatusOr<CelValue> Trace(const Activation& activation,
                                 google::protobuf::Arena* arena,
                                 CelEvaluationListener callback) const override;

 private:
  const ExecutionPath path_;
  const int max_iterations_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
