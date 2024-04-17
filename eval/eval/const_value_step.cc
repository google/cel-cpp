#include "eval/eval/const_value_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast_internal/expr.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/compiler_constant_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/status_macros.h"
#include "runtime/internal/convert_constant.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ast_internal::Constant;
using ::cel::runtime_internal::ConvertConstant;

class AstDirectImpl : public DirectExpressionStep {
 public:
  explicit AstDirectImpl(const Constant& value) : value_(value) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail&) const override {
    CEL_ASSIGN_OR_RETURN(result,
                         ConvertConstant(value_, frame.value_manager()));
    return absl::OkStatus();
  }

 private:
  Constant value_;
};

class ValueDirectImpl : public DirectExpressionStep {
 public:
  explicit ValueDirectImpl(cel::Value value) : value_(std::move(value)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail&) const override {
    result = value_;
    return absl::OkStatus();
  }

 private:
  cel::Value value_;
};

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateConstValueDirectStep(
    const Constant& value) {
  return std::make_unique<AstDirectImpl>(value);
}

std::unique_ptr<DirectExpressionStep> CreateConstValueDirectStep(
    cel::Value value) {
  return std::make_unique<ValueDirectImpl>(std::move(value));
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    cel::Value value, int64_t expr_id, bool comes_from_ast) {
  return std::make_unique<CompilerConstantStep>(std::move(value), expr_id,
                                                comes_from_ast);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const Constant& value, int64_t expr_id, cel::ValueManager& value_factory,
    bool comes_from_ast) {
  CEL_ASSIGN_OR_RETURN(cel::Value converted_value,
                       ConvertConstant(value, value_factory));

  return std::make_unique<CompilerConstantStep>(std::move(converted_value),
                                                expr_id, comes_from_ast);
}

}  // namespace google::api::expr::runtime
