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
  builder->GetRegistry()->set_partial_string_match(
      options.partial_string_match);
  builder->set_constant_folding(options.constant_folding,
                                options.constant_arena);
  return std::move(builder);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
