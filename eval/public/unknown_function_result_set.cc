#include "eval/public/unknown_function_result_set.h"

#include <type_traits>

#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

// Forward declare.
bool CelValueEqual(const CelValue lhs, const CelValue rhs);

// Default to operator==
template <typename T>
bool CelValueEqualImpl(T lhs, T rhs) {
  return lhs == rhs;
}

// List equality specialization. Test that the lists are in-order elementwise
// equal.
template <>
bool CelValueEqualImpl<const CelList*>(const CelList* lhs, const CelList* rhs) {
  if (lhs->size() != rhs->size()) {
    return false;
  }
  for (int i = 0; i < rhs->size(); i++) {
    if (!CelValueEqual(lhs->operator[](i), rhs->operator[](i))) {
      return false;
    }
  }
  return true;
}

// Map equality specialization. Compare that two maps have exactly the same
// key/value pairs.
template <>
bool CelValueEqualImpl<const CelMap*>(const CelMap* lhs, const CelMap* rhs) {
  if (lhs->size() != rhs->size()) {
    return false;
  }
  const CelList* key_set = rhs->ListKeys();
  for (int i = 0; i < key_set->size(); i++) {
    CelValue key = key_set->operator[](i);
    CelValue rhs_value = rhs->operator[](key).value();
    auto maybe_lhs_value = lhs->operator[](key);
    if (!maybe_lhs_value.has_value()) {
      return false;
    }
    if (!CelValueEqual(maybe_lhs_value.value(), rhs_value)) {
      return false;
    }
  }
  return true;
}

// Visitor for implementing comparing the underlying value that two CelValues
// are wrapping. The visitor unwraps the lhs then tries to get the rhs
// underlying value if it is the same type as the lhs.
struct LhsCompareVisitor {
  CelValue rhs;

  LhsCompareVisitor(CelValue rhs) : rhs(rhs) {}

  template <typename T>
  bool operator()(T lhs_value) {
    T rhs_value;
    bool is_same_type = rhs.GetValue(&rhs_value);
    if (!is_same_type) {
      return false;
    }
    return CelValueEqualImpl<T>(lhs_value, rhs_value);
  }
};

// This is a slightly different implementation than provided for the cel
// evaluator. Differences are:
//
// - this implementation doesn't need to support error forwarding in the same
//   way -- this should only be used for situations when we can invoke the
//   function. i.e. the function must specify that it consumes errors and/or
//   unknown sets for them to appear in the arg list.
// - this implementation defines equality between messages based on ptr identity
bool CelValueEqual(const CelValue lhs, const CelValue rhs) {
  if (lhs.type() != rhs.type()) {
    return false;
  }
  return lhs.Visit<bool>(LhsCompareVisitor(rhs));
}

// Tests that two descriptors are equal (name, receiver call style, arg types).
//
// Argument type Any is not treated specially. For example:
// {"f", false, {kAny}} != {"f", false, {kInt64}}
bool DescriptorEqual(const CelFunctionDescriptor& lhs,
                     const CelFunctionDescriptor& rhs) {
  if (lhs.name() != rhs.name()) {
    return false;
  }

  if (lhs.receiver_style() != rhs.receiver_style()) {
    return false;
  }

  if (lhs.types() != rhs.types()) {
    return false;
  }

  return true;
}

}  // namespace

bool UnknownFunctionResult::IsEqualTo(
    const UnknownFunctionResult& other) const {
  if (!DescriptorEqual(descriptor_, other.descriptor())) {
    return false;
  }

  if (arguments_.size() != other.arguments().size()) {
    return false;
  }

  for (size_t i = 0; i < arguments_.size(); i++) {
    if (!CelValueEqual(arguments_[i], other.arguments()[i])) {
      return false;
    }
  }

  return true;
}

// Implementation for merge constructor.
UnknownFunctionResultSet::UnknownFunctionResultSet(
    const UnknownFunctionResultSet& lhs, const UnknownFunctionResultSet& rhs)
    : unknown_function_results_(lhs.unknown_function_results()) {
  unknown_function_results_.reserve(lhs.unknown_function_results().size() +
                                    rhs.unknown_function_results().size());
  for (const UnknownFunctionResult* call : rhs.unknown_function_results()) {
    Add(call);
  }
}

void UnknownFunctionResultSet::Add(const UnknownFunctionResult* result) {
  for (const UnknownFunctionResult* existing_result :
       unknown_function_results()) {
    if (result->IsEqualTo(*existing_result)) {
      return;
    }
  }
  unknown_function_results_.push_back(result);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
