#include "eval/public/extension_func_registrar.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

::cel_base::Status RegisterExtensionFunctions(CelFunctionRegistry*) {
  return ::cel_base::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
