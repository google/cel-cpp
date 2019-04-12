#include "eval/public/cel_expr_builder_factory.h"
#include "eval/compiler/flat_expr_builder.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    bool shortcircuiting) {
  auto builder = absl::make_unique<FlatExprBuilder>();
  builder->set_shortcircuiting(shortcircuiting);
  return std::move(builder);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
