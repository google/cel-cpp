#include "eval/public/unknown_function_result_set.h"

#include <type_traits>

#include "absl/container/btree_set.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/set_util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

// Tests that lhs descriptor is less than (name, receiver call style,
// arg types).
// Argument type Any is not treated specially. For example:
// {"f", false, {kAny}} > {"f", false, {kInt64}}
bool DescriptorLessThan(const CelFunctionDescriptor& lhs,
                        const CelFunctionDescriptor& rhs) {
  if (lhs.name() < rhs.name()) {
    return true;
  }
  if (lhs.name() > rhs.name()) {
    return false;
  }

  if (lhs.receiver_style() < rhs.receiver_style()) {
    return true;
  }
  if (lhs.receiver_style() > rhs.receiver_style()) {
    return false;
  }

  if (lhs.types() >= rhs.types()) {
    return false;
  }

  return true;
}

bool UnknownFunctionResultLessThan(const UnknownFunctionResult& lhs,
                                   const UnknownFunctionResult& rhs) {
  if (DescriptorLessThan(lhs.descriptor(), rhs.descriptor())) {
    return true;
  }
  if (DescriptorLessThan(rhs.descriptor(), lhs.descriptor())) {
    return false;
  }

  if (lhs.arguments().size() < rhs.arguments().size()) {
    return true;
  }

  if (lhs.arguments().size() > rhs.arguments().size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.arguments().size(); i++) {
    if (CelValueLessThan(lhs.arguments()[i], rhs.arguments()[i])) {
      return true;
    }
    if (CelValueLessThan(rhs.arguments()[i], lhs.arguments()[i])) {
      return false;
    }
  }

  // equal
  return false;
}

}  // namespace

bool UnknownFunctionComparator::operator()(
    const UnknownFunctionResult* lhs, const UnknownFunctionResult* rhs) const {
  return UnknownFunctionResultLessThan(*lhs, *rhs);
}

bool UnknownFunctionResult::IsEqualTo(
    const UnknownFunctionResult& other) const {
  return !(UnknownFunctionResultLessThan(*this, other) ||
           UnknownFunctionResultLessThan(other, *this));
}

// Implementation for merge constructor.
UnknownFunctionResultSet::UnknownFunctionResultSet(
    const UnknownFunctionResultSet& lhs, const UnknownFunctionResultSet& rhs)
    : unknown_function_results_(lhs.unknown_function_results()) {
  for (const UnknownFunctionResult* call : rhs.unknown_function_results()) {
    unknown_function_results_.insert(call);
  }
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
