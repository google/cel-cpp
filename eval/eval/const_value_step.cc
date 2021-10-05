#include "eval/eval/const_value_step.h"

#include <cstdint>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/statusor.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/structs/cel_proto_wrapper.h"

namespace google::api::expr::runtime {

using ::google::api::expr::v1alpha1::Constant;

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

absl::optional<CelValue> ConvertConstant(const Constant* const_expr) {
  CelValue value = CelValue::CreateNull();
  switch (const_expr->constant_kind_case()) {
    case Constant::kNullValue:
      value = CelValue::CreateNull();
      break;
    case Constant::kBoolValue:
      value = CelValue::CreateBool(const_expr->bool_value());
      break;
    case Constant::kInt64Value:
      value = CelValue::CreateInt64(const_expr->int64_value());
      break;
    case Constant::kUint64Value:
      value = CelValue::CreateUint64(const_expr->uint64_value());
      break;
    case Constant::kDoubleValue:
      value = CelValue::CreateDouble(const_expr->double_value());
      break;
    case Constant::kStringValue:
      value = CelValue::CreateString(&const_expr->string_value());
      break;
    case Constant::kBytesValue:
      value = CelValue::CreateBytes(&const_expr->bytes_value());
      break;
    case Constant::kDurationValue:
      value = CelProtoWrapper::CreateDuration(&const_expr->duration_value());
      break;
    case Constant::kTimestampValue:
      value = CelProtoWrapper::CreateTimestamp(&const_expr->timestamp_value());
      break;
    default:
      // constant with no kind specified
      return {};
      break;
  }
  return value;
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    CelValue value, int64_t expr_id, bool comes_from_ast) {
  return absl::make_unique<ConstValueStep>(value, expr_id, comes_from_ast);
}

// Factory method for Constant(Enum value) - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const google::protobuf::EnumValueDescriptor* value_descriptor, int64_t expr_id) {
  return absl::make_unique<ConstValueStep>(
      CelValue::CreateInt64(value_descriptor->number()), expr_id, false);
}

}  // namespace google::api::expr::runtime
