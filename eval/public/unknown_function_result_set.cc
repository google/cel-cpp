#include "eval/public/unknown_function_result_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Implementation for merge constructor.
UnknownFunctionResultSet::UnknownFunctionResultSet(
    const UnknownFunctionResultSet& lhs, const UnknownFunctionResultSet& rhs)
    : function_results_(lhs.function_results_) {
  for (const auto& function_result : rhs) {
    function_results_.insert(function_result);
  }
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
