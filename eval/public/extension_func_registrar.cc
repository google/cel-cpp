#include "eval/public/extension_func_registrar.h"

#include "eval/public/cel_function_registry.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::Status RegisterExtensionFunctions(CelFunctionRegistry*) {
  return absl::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
