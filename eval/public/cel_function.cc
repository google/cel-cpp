#include "eval/public/cel_function.h"

namespace google::api::expr::runtime {

bool CelFunctionDescriptor::ShapeMatches(
    bool receiver_style, const std::vector<CelValue::Type>& types) const {
  if (receiver_style_ != receiver_style) {
    return false;
  }

  if (types_.size() != types.size()) {
    return false;
  }

  for (size_t i = 0; i < types_.size(); i++) {
    CelValue::Type this_type = types_[i];
    CelValue::Type other_type = types[i];
    if (this_type != CelValue::Type::kAny &&
        other_type != CelValue::Type::kAny && this_type != other_type) {
      return false;
    }
  }
  return true;
}

bool CelFunction::MatchArguments(absl::Span<const CelValue> arguments) const {
  auto types_size = descriptor().types().size();

  if (types_size != arguments.size()) {
    return false;
  }
  for (size_t i = 0; i < types_size; i++) {
    const auto& value = arguments[i];
    CelValue::Type arg_type = descriptor().types()[i];
    if (value.type() != arg_type && arg_type != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

}  // namespace google::api::expr::runtime
