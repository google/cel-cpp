#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_SET_H_

#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_function_result_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Class representing a collection of unknowns from a single evaluation pass of
// a CEL expression.
class UnknownSet {
 public:
  // Initilization specifying subcontainers
  explicit UnknownSet(
      const google::api::expr::runtime::UnknownAttributeSet& attrs)
      : unknown_attributes_(attrs) {}
  explicit UnknownSet(const UnknownFunctionResultSet& function_results)
      : unknown_function_results_(function_results) {}
  UnknownSet(const UnknownAttributeSet& attrs,
             const UnknownFunctionResultSet& function_results)
      : unknown_attributes_(attrs),
        unknown_function_results_(function_results) {}
  // Initialization for empty set
  UnknownSet() {}
  // Merge constructor
  UnknownSet(const UnknownSet& set1, const UnknownSet& set2)
      : unknown_attributes_(set1.unknown_attributes(),
                            set2.unknown_attributes()),
        unknown_function_results_(set1.unknown_function_results(),
                                  set2.unknown_function_results()) {}

  const UnknownAttributeSet& unknown_attributes() const {
    return unknown_attributes_;
  }
  const UnknownFunctionResultSet& unknown_function_results() const {
    return unknown_function_results_;
  }

 private:
  UnknownAttributeSet unknown_attributes_;
  UnknownFunctionResultSet unknown_function_results_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_SET_H_
