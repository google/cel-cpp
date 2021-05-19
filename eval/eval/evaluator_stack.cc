#include "eval/eval/evaluator_stack.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

void EvaluatorStack::Clear() {
  for (auto& v : stack_) {
    v = CelValue();
  }
  for (auto& attr : attribute_stack_) {
    attr = AttributeTrail();
  }

  current_size_ = 0;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
