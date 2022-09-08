#include "eval/eval/create_list_step.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/eval/expression_step_base.h"
#include "eval/eval/mutable_list_impl.h"
#include "eval/public/containers/container_backed_list_impl.h"

namespace google::api::expr::runtime {

namespace {

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

  CelValue result;
  for (const auto& arg : args) {
    if (arg.IsError()) {
      result = arg;
      frame->value_stack().Pop(list_size_);
      frame->value_stack().Push(result);
      return absl::OkStatus();
    }
  }

  const UnknownSet* unknown_set = nullptr;
  if (frame->enable_unknowns()) {
    unknown_set = frame->attribute_utility().MergeUnknowns(
        args, frame->value_stack().GetAttributeSpan(list_size_),
        /*initial_set=*/nullptr,
        /*use_partial=*/true);
    if (unknown_set != nullptr) {
      result = CelValue::CreateUnknownSet(unknown_set);
      frame->value_stack().Pop(list_size_);
      frame->value_stack().Push(result);
      return absl::OkStatus();
    }
  }

  CelList* cel_list;
  if (immutable_) {
    cel_list = frame->memory_manager()
                   .New<ContainerBackedListImpl>(
                       std::vector<CelValue>(args.begin(), args.end()))
                   .release();
  } else {
    cel_list = frame->memory_manager()
                   .New<MutableListImpl>(
                       std::vector<CelValue>(args.begin(), args.end()))
                   .release();
  }
  result = CelValue::CreateList(cel_list);
  frame->value_stack().Pop(list_size_);
  frame->value_stack().Push(result);
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateListStep(
    const cel::ast::internal::CreateList& create_list_expr, int64_t expr_id) {
  return absl::make_unique<CreateListStep>(
      expr_id, create_list_expr.elements().size(), /*immutable=*/true);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateMutableListStep(
    const cel::ast::internal::CreateList& create_list_expr, int64_t expr_id) {
  return absl::make_unique<CreateListStep>(
      expr_id, create_list_expr.elements().size(), /*immutable=*/false);
}

}  // namespace google::api::expr::runtime
