#include "eval/eval/create_list_step.h"
#include "eval/eval/container_backed_list_impl.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

class CreateListStep : public ExpressionStepBase {
 public:
  CreateListStep(int64_t expr_id, int list_size)
      : ExpressionStepBase(expr_id), list_size_(list_size) {}

  cel_base::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  int list_size_;
};

cel_base::Status CreateListStep::Evaluate(ExecutionFrame* frame) const {
  if (list_size_ < 0) {
    return cel_base::Status(cel_base::StatusCode::kInternal,
                        "CreateListStep: list size is <0");
  }

  if (!frame->value_stack().HasEnough(list_size_)) {
    return cel_base::Status(cel_base::StatusCode::kInternal,
                        "CreateListStep: stack undeflow");
  }

  auto args = frame->value_stack().GetSpan(list_size_);

  CelList* cel_list = google::protobuf::Arena::Create<ContainerBackedListImpl>(
      frame->arena(), std::vector<CelValue>(args.begin(), args.end()));

  frame->value_stack().Pop(list_size_);
  frame->value_stack().Push(CelValue::CreateList(cel_list));

  return cel_base::OkStatus();
}

}  // namespace

// Factory method for CreateList - based Execution step
cel_base::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateListStep(
    const google::api::expr::v1alpha1::Expr::CreateList* create_list_expr,
    int64_t expr_id) {
  std::unique_ptr<ExpressionStep> step = absl::make_unique<CreateListStep>(
      expr_id, create_list_expr->elements_size());
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
