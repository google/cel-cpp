#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BUILTIN_FUNC_REGISTRAR_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BUILTIN_FUNC_REGISTRAR_H_

#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

cel_base::Status RegisterBuiltinFunctions(
    CelFunctionRegistry* registry,
    const InterpreterOptions& options = InterpreterOptions());

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BUILTIN_FUNC_REGISTRAR_H_
