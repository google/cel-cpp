#include "eval/eval/const_value_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "base/ast_internal.h"
#include "eval/eval/compiler_constant_step.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/interop.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ast::internal::Constant;

class ConstValueStep : public ExpressionStepBase {
 public:
  ConstValueStep(const Constant& expr, int64_t expr_id, bool comes_from_ast)
      : ExpressionStepBase(expr_id, comes_from_ast),
        const_expr_(expr),
        value_(ConvertConstant(const_expr_)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  // Maintain a copy of the source constant to avoid lifecycle dependence on the
  // ast after planning.
  cel::ast::internal::Constant const_expr_;
  cel::Handle<cel::Value> value_;
};

absl::Status ConstValueStep::Evaluate(ExecutionFrame* frame) const {
  frame->value_stack().Push(value_);

  return absl::OkStatus();
}

}  // namespace

cel::Handle<cel::Value> ConvertConstant(
    const cel::ast::internal::Constant& const_expr) {
  struct {
    cel::Handle<cel::Value> operator()(
        const cel::ast::internal::NullValue& value) {
      return cel::interop_internal::CreateNullValue();
    }
    cel::Handle<cel::Value> operator()(bool value) {
      return cel::interop_internal::CreateBoolValue(value);
    }
    cel::Handle<cel::Value> operator()(int64_t value) {
      return cel::interop_internal::CreateIntValue(value);
    }
    cel::Handle<cel::Value> operator()(uint64_t value) {
      return cel::interop_internal::CreateUintValue(value);
    }
    cel::Handle<cel::Value> operator()(double value) {
      return cel::interop_internal::CreateDoubleValue(value);
    }
    cel::Handle<cel::Value> operator()(const std::string& value) {
      return cel::interop_internal::CreateStringValueFromView(value);
    }
    cel::Handle<cel::Value> operator()(const cel::ast::internal::Bytes& value) {
      return cel::interop_internal::CreateBytesValueFromView(value.bytes);
    }
    cel::Handle<cel::Value> operator()(const absl::Duration duration) {
      return cel::interop_internal::CreateDurationValue(duration);
    }
    cel::Handle<cel::Value> operator()(const absl::Time timestamp) {
      return cel::interop_internal::CreateTimestampValue(timestamp);
    }
  } handler;
  return absl::visit(handler, const_expr.constant_kind());
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    cel::Handle<cel::Value> value, int64_t expr_id, bool comes_from_ast) {
  return std::make_unique<CompilerConstantStep>(std::move(value), expr_id,
                                                comes_from_ast);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const Constant& value, int64_t expr_id, bool comes_from_ast) {
  return std::make_unique<ConstValueStep>(value, expr_id, comes_from_ast);
}

}  // namespace google::api::expr::runtime
