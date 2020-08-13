#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_DEBUG_STRING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_DEBUG_STRING_H_

#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace test {

// String rerpesentation of the underlying value.
std::string DebugValueString(const CelValue& value);

// String representation of the cel value. This should only be used for
// informational purposes and the exact format may change. In particular,
// ordering is not guaranteed for some container types.
std::string DebugString(const CelValue& value);

}  // namespace test
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_DEBUG_STRING_H_
