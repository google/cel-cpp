#include "eval/public/cel_expr_builder_factory.h"

#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/cel_options.h"

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
  builder->set_fail_on_warnings(options.fail_on_warnings);

  switch (options.unknown_processing) {
    case UnknownProcessingOptions::kAttributeAndFunction:
      builder->set_enable_unknown_function_results(true);
      builder->set_enable_unknowns(true);
      break;
    case UnknownProcessingOptions::kAttributeOnly:
      builder->set_enable_unknowns(true);
      break;
    case UnknownProcessingOptions::kDisabled:
      break;
  }

  builder->set_enable_missing_attribute_errors(
      options.enable_missing_attribute_errors);

  return std::move(builder);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
