#include "eval/public/cel_function.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google::api::expr::runtime {

bool CelFunctionDescriptor::ShapeMatches(
    bool receiver_style, const std::vector<CelValue::Type>& types) const {
  if (this->receiver_style() != receiver_style) {
    return false;
  }

  if (this->types().size() != types.size()) {
    return false;
  }

  for (size_t i = 0; i < this->types().size(); i++) {
    CelValue::Type this_type = this->types()[i];
    CelValue::Type other_type = types[i];
    if (this_type != CelValue::Type::kAny &&
        other_type != CelValue::Type::kAny && this_type != other_type) {
      return false;
    }
  }
  return true;
}

bool CelFunctionDescriptor::operator==(
    const CelFunctionDescriptor& other) const {
  return impl_.get() == other.impl_.get() ||
         (name() == other.name() &&
          receiver_style() == other.receiver_style() &&
          types().size() == other.types().size() &&
          std::equal(types().begin(), types().end(), other.types().begin()));
}

bool CelFunctionDescriptor::operator<(
    const CelFunctionDescriptor& other) const {
  if (impl_.get() == other.impl_.get()) {
    return false;
  }
  if (name() < other.name()) {
    return true;
  }
  if (name() != other.name()) {
    return false;
  }
  if (receiver_style() < other.receiver_style()) {
    return true;
  }
  if (receiver_style() != other.receiver_style()) {
    return false;
  }
  auto lhs_begin = types().begin();
  auto lhs_end = types().end();
  auto rhs_begin = other.types().begin();
  auto rhs_end = other.types().end();
  while (lhs_begin != lhs_end && rhs_begin != rhs_end) {
    if (*lhs_begin < *rhs_begin) {
      return true;
    }
    if (!(*lhs_begin == *rhs_begin)) {
      return false;
    }
    lhs_begin++;
    rhs_begin++;
  }
  if (lhs_begin == lhs_end && rhs_begin == rhs_end) {
    // Neither has any elements left, they are equal.
    return false;
  }
  if (lhs_begin == lhs_end) {
    // Left has no more elements. Right is greater.
    return true;
  }
  // Right has no more elements. Left is greater.
  ABSL_ASSERT(rhs_begin == rhs_end);
  return false;
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
