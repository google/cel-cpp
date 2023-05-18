#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_SET_H_

#include <memory>
#include <utility>

#include "base/internal/unknown_set.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_function_result_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Class representing a collection of unknowns from a single evaluation pass of
// a CEL expression.
using UnknownSet = ::cel::base_internal::UnknownSet;

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_SET_H_
