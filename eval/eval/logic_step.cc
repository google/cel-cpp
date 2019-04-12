#include "eval/eval/logic_step.h"

#include "eval/eval/expression_step_base.h"
#include "absl/strings/str_cat.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

class LogicalOpStep : public ExpressionStepBase {
 public:
  enum class OpType { AND, OR };

  // Constructs FunctionStep that uses overloads specified.
  LogicalOpStep(OpType op_type, const google::api::expr::v1alpha1::Expr* expr)
      : ExpressionStepBase(expr), op_type_(op_type) {
    shortcircuit_ = (op_type_ == OpType::OR);
  }

  util::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  util::Status Calculate(absl::Span<const CelValue> args,
                         CelValue* result) const {
    bool bool_args[2];
    bool has_bool_args[2];

    for (int i = 0; i < args.size(); i++) {
      has_bool_args[i] = args[i].GetValue(bool_args + i);
      if (has_bool_args[i] && shortcircuit_ == bool_args[i]) {
        *result = CelValue::CreateBool(bool_args[i]);
        return util::OkStatus();
      }
    }

    if (has_bool_args[0] && has_bool_args[1]) {
      switch (op_type_) {
        case OpType::AND:
          *result = CelValue::CreateBool(bool_args[0] && bool_args[1]);
          return util::OkStatus();
          break;
        case OpType::OR:
          *result = CelValue::CreateBool(bool_args[0] || bool_args[1]);
          return util::OkStatus();
          break;
      }
    } else {
      if (args[0].IsError()) {
        *result = args[0];
      } else if (args[1].IsError()) {
        *result = args[1];
      } else {
        return util::MakeStatus(google::rpc::Code::INTERNAL,
                            "Unsupported type supplied for logical operation");
      }

      return util::OkStatus();
    }
  }

  const OpType op_type_;
  bool shortcircuit_;
};

util::Status LogicalOpStep::Evaluate(ExecutionFrame* frame) const {
  // Must have 2 or more values on the stack.
  if (!frame->value_stack().HasEnough(2)) {
    return util::MakeStatus(google::rpc::Code::INTERNAL, "Value stack underflow");
  }

  // Create Span object that contains input arguments to the function.
  auto args = frame->value_stack().GetSpan(2);

  CelValue value;

  auto status = Calculate(args, &value);
  if (!util::IsOk(status)) {
    return status;
  }

  frame->value_stack().Pop(args.size());
  frame->value_stack().Push(value);

  return status;
}

}  // namespace

// Factory method for "And" Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateAndStep(
    const google::api::expr::v1alpha1::Expr* expr) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<LogicalOpStep>(LogicalOpStep::OpType::AND, expr);

  return std::move(step);
}

// Factory method for "Or" Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateOrStep(
    const google::api::expr::v1alpha1::Expr* expr) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<LogicalOpStep>(LogicalOpStep::OpType::OR, expr);

  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
