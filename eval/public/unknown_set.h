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

class AttributeUtility;

// Class representing a collection of unknowns from a single evaluation pass of
// a CEL expression.
class UnknownSet {
 private:
  using Impl = ::cel::base_internal::UnknownSetImpl;

 public:
  UnknownSet() = default;

  // Initilization specifying subcontainers
  explicit UnknownSet(
      const google::api::expr::runtime::UnknownAttributeSet& attrs)
      : impl_(std::make_shared<Impl>(attrs)) {}

  explicit UnknownSet(const UnknownFunctionResultSet& function_results)
      : impl_(std::make_shared<Impl>(function_results)) {}

  UnknownSet(const UnknownAttributeSet& attrs,
             const UnknownFunctionResultSet& function_results)
      : impl_(std::make_shared<Impl>(attrs, function_results)) {}

  // Initialization for empty set
  // Merge constructor
  UnknownSet(const UnknownSet& set1, const UnknownSet& set2)
      : UnknownSet(set1.unknown_attributes(), set2.unknown_function_results()) {
    Add(set2);
  }

  const UnknownAttributeSet& unknown_attributes() const {
    return impl_ != nullptr ? impl_->attributes
                            : ::cel::base_internal::EmptyAttributeSet();
  }
  const UnknownFunctionResultSet& unknown_function_results() const {
    return impl_ != nullptr ? impl_->function_results
                            : ::cel::base_internal::EmptyFunctionResultSet();
  }

  bool operator==(const UnknownSet& other) const {
    return this == &other ||
           (unknown_attributes() == other.unknown_attributes() &&
            unknown_function_results() == other.unknown_function_results());
  }

  bool operator!=(const UnknownSet& other) const { return !operator==(other); }

 private:
  friend class AttributeUtility;

  explicit UnknownSet(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

  void Add(const UnknownSet& other) {
    if (impl_ == nullptr) {
      impl_ = std::make_shared<Impl>();
    }
    impl_->attributes.Add(other.unknown_attributes());
    impl_->function_results.Add(other.unknown_function_results());
  }

  std::shared_ptr<Impl> impl_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_SET_H_
