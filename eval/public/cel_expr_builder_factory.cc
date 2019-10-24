#include "eval/public/cel_expr_builder_factory.h"
#include "eval/compiler/flat_expr_builder.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const InterpreterOptions& options) {
  auto builder = absl::make_unique<FlatExprBuilder>();
  builder->set_shortcircuiting(options.short_circuiting);
  builder->set_constant_folding(options.constant_folding,
                                options.constant_arena);
  builder->set_enable_comprehension(options.enable_comprehension);
  builder->set_comprehension_max_iterations(
      options.comprehension_max_iterations);

  return std::move(builder);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
