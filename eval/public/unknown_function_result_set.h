#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_

#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Represents a function result that is unknown at the time of execution. This
// allows for lazy evaluation of expensive functions.
class UnknownFunctionResult {
 public:
  UnknownFunctionResult(const CelFunctionDescriptor& descriptor, int64_t expr_id)
      : descriptor_(descriptor), expr_id_(expr_id) {}

  // The descriptor of the called function that return Unknown.
  const CelFunctionDescriptor& descriptor() const { return descriptor_; }

  // The id of the |Expr| that triggered the function call step. Provided
  // informationally -- if two different |Expr|s generate the same unknown call,
  // they will be treated as the same unknown function result.
  int64_t call_expr_id() const { return expr_id_; }

  // Equality operator provided for testing. Compatible with set less-than
  // comparator.
  // Compares descriptor then arguments elementwise.
  bool IsEqualTo(const UnknownFunctionResult& other) const;

  // TODO(issues/5): re-implement argument capture

 private:
  CelFunctionDescriptor descriptor_;
  int64_t expr_id_;
};

// Comparator for set semantics.
struct UnknownFunctionComparator {
  bool operator()(const UnknownFunctionResult*,
                  const UnknownFunctionResult*) const;
};

// Represents a collection of unknown function results at a particular point in
// execution. Execution should advance further if this set of unknowns are
// provided. It may not advance if only a subset are provided.
// Set semantics use |IsEqualTo()| defined on |UnknownFunctionResult|.
class UnknownFunctionResultSet {
 public:
  // Empty set
  UnknownFunctionResultSet() {}

  // Merge constructor -- effectively union(lhs, rhs).
  UnknownFunctionResultSet(const UnknownFunctionResultSet& lhs,
                           const UnknownFunctionResultSet& rhs);

  // Initialize with a single UnknownFunctionResult.
  UnknownFunctionResultSet(const UnknownFunctionResult* initial)
      : unknown_function_results_{initial} {}

  using Container =
      absl::btree_set<const UnknownFunctionResult*, UnknownFunctionComparator>;

  const Container& unknown_function_results() const {
    return unknown_function_results_;
  }

 private:
  Container unknown_function_results_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_
