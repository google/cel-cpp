#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_

#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/btree_set.h"
#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Represents a function result that is unknown at the time of execution. This
// allows for lazy evaluation of expensive functions.
class UnknownFunctionResult final {
 public:
  UnknownFunctionResult() = default;
  UnknownFunctionResult(const UnknownFunctionResult&) = default;
  UnknownFunctionResult(UnknownFunctionResult&&) = default;
  UnknownFunctionResult& operator=(const UnknownFunctionResult&) = default;
  UnknownFunctionResult& operator=(UnknownFunctionResult&&) = default;

  UnknownFunctionResult(CelFunctionDescriptor descriptor, int64_t expr_id)
      : descriptor_(std::move(descriptor)), expr_id_(expr_id) {}

  // The descriptor of the called function that return Unknown.
  const CelFunctionDescriptor& descriptor() const { return descriptor_; }

  // The id of the |Expr| that triggered the function call step. Provided
  // informationally -- if two different |Expr|s generate the same unknown call,
  // they will be treated as the same unknown function result.
  int64_t call_expr_id() const { return expr_id_; }

  // Equality operator provided for testing. Compatible with set less-than
  // comparator.
  // Compares descriptor then arguments elementwise.
  bool IsEqualTo(const UnknownFunctionResult& other) const {
    return descriptor() == other.descriptor();
  }

  // TODO(issues/5): re-implement argument capture

 private:
  CelFunctionDescriptor descriptor_;
  int64_t expr_id_;
};

inline bool operator==(const UnknownFunctionResult& lhs,
                       const UnknownFunctionResult& rhs) {
  return lhs.IsEqualTo(rhs);
}

inline bool operator<(const UnknownFunctionResult& lhs,
                      const UnknownFunctionResult& rhs) {
  return lhs.descriptor() < rhs.descriptor();
}

class AttributeUtility;
class UnknownSet;

// Represents a collection of unknown function results at a particular point in
// execution. Execution should advance further if this set of unknowns are
// provided. It may not advance if only a subset are provided.
// Set semantics use |IsEqualTo()| defined on |UnknownFunctionResult|.
class UnknownFunctionResultSet final {
 private:
  using Container = absl::btree_set<UnknownFunctionResult>;

 public:
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using iterator = typename Container::const_iterator;
  using const_iterator = typename Container::const_iterator;

  UnknownFunctionResultSet() = default;
  UnknownFunctionResultSet(const UnknownFunctionResultSet&) = default;
  UnknownFunctionResultSet(UnknownFunctionResultSet&&) = default;
  UnknownFunctionResultSet& operator=(const UnknownFunctionResultSet&) =
      default;
  UnknownFunctionResultSet& operator=(UnknownFunctionResultSet&&) = default;

  // Merge constructor -- effectively union(lhs, rhs).
  UnknownFunctionResultSet(const UnknownFunctionResultSet& lhs,
                           const UnknownFunctionResultSet& rhs);

  // Initialize with a single UnknownFunctionResult.
  explicit UnknownFunctionResultSet(UnknownFunctionResult initial)
      : function_results_{std::move(initial)} {}

  UnknownFunctionResultSet(std::initializer_list<UnknownFunctionResult> il)
      : function_results_(il) {}

  iterator begin() const { return function_results_.begin(); }

  const_iterator cbegin() const { return function_results_.cbegin(); }

  iterator end() const { return function_results_.end(); }

  const_iterator cend() const { return function_results_.cend(); }

  size_type size() const { return function_results_.size(); }

  bool empty() const { return function_results_.empty(); }

  bool operator==(const UnknownFunctionResultSet& other) const {
    return this == &other || function_results_ == other.function_results_;
  }

  bool operator!=(const UnknownFunctionResultSet& other) const {
    return !operator==(other);
  }

 private:
  friend class AttributeUtility;
  friend class UnknownSet;

  void Add(const UnknownFunctionResult& function_result) {
    function_results_.insert(function_result);
  }

  void Add(const UnknownFunctionResultSet& other) {
    for (const auto& function_result : other) {
      Add(function_result);
    }
  }

  Container function_results_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_
