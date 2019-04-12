#include "eval/public/extension_func_registrar.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

util::Status RegisterExtensionFunctions(CelFunctionRegistry* registry) {
  return util::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
