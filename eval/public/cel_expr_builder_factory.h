#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_

#include "eval/public/cel_expression.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Interpreter options for controlling evaluation and builtin functions.
struct InterpreterOptions {
  InterpreterOptions()
      : short_circuiting(true),
        partial_string_match(false),
        constant_folding(false),
        constant_arena(nullptr) {}

  // Enable short-circuiting of the logical operator evaluation. If enabled,
  // AND, OR, and TERNARY do not evaluate the entire expression once the the
  // resulting value is known from the left-hand side.
  bool short_circuiting = true;

  // Indicate whether to use partial or full string regex matching.
  // Should be enabled to conform with the CEL specification.
  bool partial_string_match = false;

  // Enable constant folding during the expression creation. If enabled,
  // an arena must be provided for constant generation.
  bool constant_folding = false;
  google::protobuf::Arena* constant_arena;
};

// Factory creates CelExpressionBuilder implementation for public use.
std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const InterpreterOptions& options = InterpreterOptions());

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_
