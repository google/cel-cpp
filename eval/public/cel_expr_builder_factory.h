#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_

#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory creates CelExpressionBuilder implementation for public use.
std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const InterpreterOptions& options = InterpreterOptions());

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_
