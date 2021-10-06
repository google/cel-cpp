#include "eval/eval/evaluator_stack.h"

namespace google::api::expr::runtime {

void EvaluatorStack::Clear() {
  for (auto& v : stack_) {
    v = CelValue();
  }
  for (auto& attr : attribute_stack_) {
    attr = AttributeTrail();
  }

  current_size_ = 0;
}

}  // namespace google::api::expr::runtime
