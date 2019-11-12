#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

bool CelFunction::MatchArguments(absl::Span<const CelValue> arguments) const {
  auto types_size = descriptor().types.size();

  if (types_size != arguments.size()) {
    return false;
  }

  for (size_t i = 0; i < types_size; i++) {
    const auto& value = arguments[i];
    CelValue::Type arg_type = descriptor().types[i];
    if (value.type() != arg_type && arg_type != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
