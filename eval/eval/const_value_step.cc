#include "eval/eval/const_value_step.h"

#include <cstdint>
#include <memory>
#include <string>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "base/ast.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_value.h"
#include "internal/proto_time_encoding.h"

namespace google::api::expr::runtime {

namespace {

class ConstValueStep : public ExpressionStepBase {
 public:
  ConstValueStep(const CelValue& value, int64_t expr_id, bool comes_from_ast)
      : ExpressionStepBase(expr_id, comes_from_ast), value_(value) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  CelValue value_;
};

absl::Status ConstValueStep::Evaluate(ExecutionFrame* frame) const {
  frame->value_stack().Push(value_);

  return absl::OkStatus();
}

}  // namespace

absl::optional<CelValue> ConvertConstant(
    const cel::ast::internal::Constant& const_expr) {
  struct {
    CelValue operator()(const cel::ast::internal::NullValue& value) {
      return CelValue::CreateNull();
    }
    CelValue operator()(bool value) { return CelValue::CreateBool(value); }
    CelValue operator()(int64_t value) { return CelValue::CreateInt64(value); }
    CelValue operator()(uint64_t value) {
      return CelValue::CreateUint64(value);
    }
    CelValue operator()(double value) { return CelValue::CreateDouble(value); }
    CelValue operator()(const std::string& value) {
      return CelValue::CreateString(&value);
    }
    CelValue operator()(const cel::ast::internal::Bytes& value) {
      return CelValue::CreateBytes(&value.bytes);
    }
    CelValue operator()(const absl::Duration duration) {
      return CelValue::CreateDuration(duration);
    }
    CelValue operator()(const absl::Time timestamp) {
      return CelValue::CreateTimestamp(timestamp);
    }
  } handler;
  return absl::visit(handler, const_expr.constant_kind());
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    CelValue value, int64_t expr_id, bool comes_from_ast) {
  return std::make_unique<ConstValueStep>(value, expr_id, comes_from_ast);
}

// Factory method for Constant(Enum value) - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const google::protobuf::EnumValueDescriptor* value_descriptor, int64_t expr_id) {
  return std::make_unique<ConstValueStep>(
      CelValue::CreateInt64(value_descriptor->number()), expr_id, false);
}

}  // namespace google::api::expr::runtime
