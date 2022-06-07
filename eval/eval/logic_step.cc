#include "eval/eval/logic_step.h"

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"

namespace google::api::expr::runtime {

namespace {

class LogicalOpStep : public ExpressionStepBase {
 public:
  enum class OpType { AND, OR };

  // Constructs FunctionStep that uses overloads specified.
  LogicalOpStep(OpType op_type, int64_t expr_id)
      : ExpressionStepBase(expr_id), op_type_(op_type) {
    shortcircuit_ = (op_type_ == OpType::OR);
  }

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status Calculate(ExecutionFrame* frame, absl::Span<const CelValue> args,
                         CelValue* result) const {
    bool bool_args[2];
    bool has_bool_args[2];

    for (size_t i = 0; i < args.size(); i++) {
      has_bool_args[i] = args[i].GetValue(bool_args + i);
      if (has_bool_args[i] && shortcircuit_ == bool_args[i]) {
        *result = CelValue::CreateBool(bool_args[i]);
        return absl::OkStatus();
      }
    }

    if (has_bool_args[0] && has_bool_args[1]) {
      switch (op_type_) {
        case OpType::AND:
          *result = CelValue::CreateBool(bool_args[0] && bool_args[1]);
          return absl::OkStatus();
          break;
        case OpType::OR:
          *result = CelValue::CreateBool(bool_args[0] || bool_args[1]);
          return absl::OkStatus();
          break;
      }
    }

    // As opposed to regular function, logical operation treat Unknowns with
    // higher precedence than error. This is due to the fact that after Unknown
    // is resolved to actual value, it may shortcircuit and thus hide the error.
    if (frame->enable_unknowns()) {
      // Check if unknown?
      const UnknownSet* unknown_set =
          frame->attribute_utility().MergeUnknowns(args,
                                                   /*initial_set=*/nullptr);

      if (unknown_set) {
        *result = CelValue::CreateUnknownSet(unknown_set);
        return absl::OkStatus();
      }
    }

    if (args[0].IsError()) {
      *result = args[0];
      return absl::OkStatus();
    } else if (args[1].IsError()) {
      *result = args[1];
      return absl::OkStatus();
    }

    // Fallback.
    *result = CreateNoMatchingOverloadError(
        frame->memory_manager(),
        (op_type_ == OpType::OR) ? builtin::kOr : builtin::kAnd);
    return absl::OkStatus();
  }

  const OpType op_type_;
  bool shortcircuit_;
};

absl::Status LogicalOpStep::Evaluate(ExecutionFrame* frame) const {
  // Must have 2 or more values on the stack.
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  // Create Span object that contains input arguments to the function.
  auto args = frame->value_stack().GetSpan(2);

  CelValue value;

  auto status = Calculate(frame, args, &value);
  if (!status.ok()) {
    return status;
  }

  frame->value_stack().Pop(args.size());
  frame->value_stack().Push(value);

  return status;
}

}  // namespace

// Factory method for "And" Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateAndStep(int64_t expr_id) {
  return absl::make_unique<LogicalOpStep>(LogicalOpStep::OpType::AND, expr_id);
}

// Factory method for "Or" Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateOrStep(int64_t expr_id) {
  return absl::make_unique<LogicalOpStep>(LogicalOpStep::OpType::OR, expr_id);
}

}  // namespace google::api::expr::runtime
