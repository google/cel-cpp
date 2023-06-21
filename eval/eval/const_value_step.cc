#include "eval/eval/const_value_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "base/ast_internal/expr.h"
#include "base/value_factory.h"
#include "eval/eval/compiler_constant_step.h"
#include "eval/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ast_internal::Constant;
using ::cel::runtime_internal::DurationOverflowError;
using ::cel::runtime_internal::kDurationHigh;
using ::cel::runtime_internal::kDurationLow;

// Adapt AST constant to a handle.
//
// Underlying data is copied for string types to keep the program independent
// from the input AST.
//
// The evaluator assumes most ast constants are valid so we use the unchecked
// versions here.
//
// A status may still be returned if value creation fails according to the
// value_factory policy.
absl::StatusOr<cel::Handle<cel::Value>> ConvertConstant(
    const Constant& const_expr, cel::ValueFactory& value_factory) {
  struct {
    absl::StatusOr<cel::Handle<cel::Value>> operator()(
        const cel::ast_internal::NullValue& value) {
      return value_factory.GetNullValue();
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(bool value) {
      return value_factory.CreateBoolValue(value);
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(int64_t value) {
      return value_factory.CreateIntValue(value);
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(uint64_t value) {
      return value_factory.CreateUintValue(value);
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(double value) {
      return value_factory.CreateDoubleValue(value);
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(
        const std::string& value) {
      return value_factory.CreateUncheckedStringValue(value);
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(
        const cel::ast_internal::Bytes& value) {
      return value_factory.CreateBytesValue(value.bytes);
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(
        const absl::Duration duration) {
      if (duration >= kDurationHigh || duration <= kDurationLow) {
        return value_factory.CreateErrorValue(*DurationOverflowError());
      }
      return value_factory.CreateUncheckedDurationValue(duration);
    }
    absl::StatusOr<cel::Handle<cel::Value>> operator()(
        const absl::Time timestamp) {
      return value_factory.CreateUncheckedTimestampValue(timestamp);
    }
    cel::ValueFactory& value_factory;
  } handler{value_factory};
  return absl::visit(handler, const_expr.constant_kind());
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    cel::Handle<cel::Value> value, int64_t expr_id, bool comes_from_ast) {
  return std::make_unique<CompilerConstantStep>(std::move(value), expr_id,
                                                comes_from_ast);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const Constant& value, int64_t expr_id, cel::ValueFactory& value_factory,
    bool comes_from_ast) {
  CEL_ASSIGN_OR_RETURN(cel::Handle<cel::Value> converted_value,
                       ConvertConstant(value, value_factory));

  return std::make_unique<CompilerConstantStep>(std::move(converted_value),
                                                expr_id, comes_from_ast);
}

}  // namespace google::api::expr::runtime
