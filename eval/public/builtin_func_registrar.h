#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BUILTIN_FUNC_REGISTRAR_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BUILTIN_FUNC_REGISTRAR_H_

#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"

namespace google::api::expr::runtime {

absl::Status RegisterBuiltinFunctions(
    CelFunctionRegistry* registry,
    const InterpreterOptions& options = InterpreterOptions());

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BUILTIN_FUNC_REGISTRAR_H_
