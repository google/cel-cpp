#include "eval/eval/create_list_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "eval/eval/expression_step_base.h"
#include "eval/eval/mutable_list_impl.h"
#include "eval/internal/interop.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "extensions/protobuf/memory_manager.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Handle;
using ::cel::UnknownValue;
using ::cel::interop_internal::CreateLegacyListValue;
using ::cel::interop_internal::ModernValueToLegacyValueOrDie;

class CreateListStep : public ExpressionStepBase {
 public:
  CreateListStep(int64_t expr_id, int list_size, bool immutable)
      : ExpressionStepBase(expr_id),
        list_size_(list_size),
        immutable_(immutable) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  int list_size_;
  bool immutable_;
};

absl::Status CreateListStep::Evaluate(ExecutionFrame* frame) const {
  if (list_size_ < 0) {
    return absl::Status(absl::StatusCode::kInternal,
                        "CreateListStep: list size is <0");
  }

  if (!frame->value_stack().HasEnough(list_size_)) {
    return absl::Status(absl::StatusCode::kInternal,
                        "CreateListStep: stack underflow");
  }

  auto args = frame->value_stack().GetSpan(list_size_);

  cel::Handle<cel::Value> result;
  for (const auto& arg : args) {
    if (arg->Is<cel::ErrorValue>()) {
      result = arg;
      frame->value_stack().Pop(list_size_);
      frame->value_stack().Push(std::move(result));
      return absl::OkStatus();
    }
  }

  if (frame->enable_unknowns()) {
    absl::optional<Handle<UnknownValue>> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(list_size_),
            /*use_partial=*/true);
    if (unknown_set.has_value()) {
      frame->value_stack().Pop(list_size_);
      frame->value_stack().Push(std::move(unknown_set).value());
      return absl::OkStatus();
    }
  }

  auto* arena = cel::extensions::ProtoMemoryManager::CastToProtoArena(
      frame->memory_manager());

  if (immutable_) {
    // TODO(uncreated-issue/23): switch to new cel::ListValue in phase 2
    result =
        CreateLegacyListValue(google::protobuf::Arena::Create<ContainerBackedListImpl>(
            arena,
            ModernValueToLegacyValueOrDie(frame->memory_manager(), args)));
  } else {
    // TODO(uncreated-issue/23): switch to new cel::ListValue in phase 2
    result = CreateLegacyListValue(google::protobuf::Arena::Create<MutableListImpl>(
        arena, ModernValueToLegacyValueOrDie(frame->memory_manager(), args)));
  }
  frame->value_stack().Pop(list_size_);
  frame->value_stack().Push(std::move(result));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateListStep(
    const cel::ast_internal::CreateList& create_list_expr, int64_t expr_id) {
  return std::make_unique<CreateListStep>(
      expr_id, create_list_expr.elements().size(), /*immutable=*/true);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateMutableListStep(
    const cel::ast_internal::CreateList& create_list_expr, int64_t expr_id) {
  return std::make_unique<CreateListStep>(
      expr_id, create_list_expr.elements().size(), /*immutable=*/false);
}

}  // namespace google::api::expr::runtime
