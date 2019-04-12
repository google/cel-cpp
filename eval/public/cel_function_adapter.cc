#include "eval/public/cel_function_adapter.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace internal {

template <>
absl::optional<CelValue::Type> TypeCodeMatch<CelValue>() {
  return CelValue::Type::kAny;
}


}  // namespace internal

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
