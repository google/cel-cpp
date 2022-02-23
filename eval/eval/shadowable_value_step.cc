#include "eval/eval/shadowable_value_step.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::extensions::ProtoMemoryManager;

class ShadowableValueStep : public ExpressionStepBase {
 public:
  ShadowableValueStep(const std::string& identifier, const CelValue& value,
                      int64_t expr_id)
      : ExpressionStepBase(expr_id), identifier_(identifier), value_(value) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string identifier_;
  CelValue value_;
};

absl::Status ShadowableValueStep::Evaluate(ExecutionFrame* frame) const {
  // TODO(issues/5): update ValueProducer to support generic MemoryManager
  // API.
  google::protobuf::Arena* arena =
      ProtoMemoryManager::CastToProtoArena(frame->memory_manager());
  auto var = frame->activation().FindValue(identifier_, arena);
  frame->value_stack().Push(var.value_or(value_));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    const std::string& identifier, const CelValue& value, int64_t expr_id) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<ShadowableValueStep>(identifier, value, expr_id);
  return std::move(step);
}

}  // namespace google::api::expr::runtime
