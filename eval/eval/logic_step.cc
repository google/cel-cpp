#include "eval/eval/logic_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/builtins.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/values/bool_value.h"
#include "base/values/unknown_value.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::Handle;
using ::cel::Value;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

class LogicalOpStep : public ExpressionStepBase {
 public:
  enum class OpType { AND, OR };

  // Constructs FunctionStep that uses overloads specified.
  LogicalOpStep(OpType op_type, int64_t expr_id)
      : ExpressionStepBase(expr_id, true, /*stack_delta=*/-1),
        op_type_(op_type) {
    shortcircuit_ = (op_type_ == OpType::OR);
  }

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  Handle<Value> Calculate(ExecutionFrame* frame,
                          absl::Span<const Handle<Value>> args) const {
    bool bool_args[2];
    bool has_bool_args[2];

    for (size_t i = 0; i < args.size(); i++) {
      has_bool_args[i] = args[i]->Is<BoolValue>();
      if (has_bool_args[i]) {
        bool_args[i] = args[i].As<BoolValue>()->NativeValue();
        if (bool_args[i] == shortcircuit_) {
          return args[i];
        }
      }
    }

    if (has_bool_args[0] && has_bool_args[1]) {
      switch (op_type_) {
        case OpType::AND:
          return frame->value_factory().CreateBoolValue(bool_args[0] &&
                                                        bool_args[1]);
        case OpType::OR:
          return frame->value_factory().CreateBoolValue(bool_args[0] ||
                                                        bool_args[1]);
      }
    }

    // As opposed to regular function, logical operation treat Unknowns with
    // higher precedence than error. This is due to the fact that after Unknown
    // is resolved to actual value, it may short-circuit and thus hide the
    // error.
    if (frame->enable_unknowns()) {
      // Check if unknown?
      absl::optional<Handle<cel::UnknownValue>> unknown_set =
          frame->attribute_utility().MergeUnknowns(args);
      if (unknown_set.has_value()) {
        return *unknown_set;
      }
    }

    if (args[0]->Is<cel::ErrorValue>()) {
      return args[0];
    } else if (args[1]->Is<cel::ErrorValue>()) {
      return args[1];
    }

    // Fallback.
    return frame->value_factory().CreateErrorValue(
        CreateNoMatchingOverloadError(
            (op_type_ == OpType::OR) ? cel::builtin::kOr : cel::builtin::kAnd));
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
  Handle<Value> result = Calculate(frame, args);
  frame->value_stack().Pop(args.size());
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

}  // namespace

// Factory method for "And" Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateAndStep(int64_t expr_id) {
  return std::make_unique<LogicalOpStep>(LogicalOpStep::OpType::AND, expr_id);
}

// Factory method for "Or" Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateOrStep(int64_t expr_id) {
  return std::make_unique<LogicalOpStep>(LogicalOpStep::OpType::OR, expr_id);
}

}  // namespace google::api::expr::runtime
