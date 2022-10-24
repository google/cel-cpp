#include "eval/eval/shadowable_value_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::extensions::ProtoMemoryManager;

class ShadowableValueStep : public ExpressionStepBase {
 public:
  ShadowableValueStep(std::string identifier, cel::Handle<cel::Value> value,
                      int64_t expr_id)
      : ExpressionStepBase(expr_id),
        identifier_(std::move(identifier)),
        value_(std::move(value)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string identifier_;
  cel::Handle<cel::Value> value_;
};

absl::Status ShadowableValueStep::Evaluate(ExecutionFrame* frame) const {
  // TODO(issues/5): update ValueProducer to support generic MemoryManager
  // API.
  google::protobuf::Arena* arena =
      ProtoMemoryManager::CastToProtoArena(frame->memory_manager());
  auto var = frame->activation().FindValue(identifier_, arena);
  if (var.has_value()) {
    // TODO(issues/5): remove conversion after activation is migrated
    CEL_ASSIGN_OR_RETURN(auto modern_value,
                         cel::interop_internal::FromLegacyValue(arena, *var));
    frame->value_stack().Push(std::move(modern_value));
  } else {
    frame->value_stack().Push(value_);
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    std::string identifier, cel::Handle<cel::Value> value, int64_t expr_id) {
  return absl::make_unique<ShadowableValueStep>(std::move(identifier),
                                                std::move(value), expr_id);
}

}  // namespace google::api::expr::runtime
