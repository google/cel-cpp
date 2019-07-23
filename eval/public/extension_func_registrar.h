#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_EXTENSION_FUNC_REGISTRAR_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_EXTENSION_FUNC_REGISTRAR_H_

#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Register generic/widely used extension functions.
cel_base::Status RegisterExtensionFunctions(CelFunctionRegistry* registry);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_EXTENSION_FUNC_REGISTRAR_H_
