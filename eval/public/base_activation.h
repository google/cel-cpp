#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BASE_ACTIVATION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BASE_ACTIVATION_H_

#include <vector>

#include "google/protobuf/field_mask.pb.h"
#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// Base class for an activation.
class BaseActivation {
 public:
  BaseActivation() = default;

  // Non-copyable/non-assignable
  BaseActivation(const BaseActivation&) = delete;
  BaseActivation& operator=(const BaseActivation&) = delete;

  // Return a list of function overloads for the given name.
  virtual std::vector<const CelFunction*> FindFunctionOverloads(
      absl::string_view) const = 0;

  // Provide the value that is bound to the name, if found.
  // arena parameter is provided to support the case when we want to pass the
  // ownership of returned object ( Message/List/Map ) to Evaluator.
  virtual absl::optional<CelValue> FindValue(absl::string_view,
                                             google::protobuf::Arena*) const = 0;

  ABSL_DEPRECATED(
      "No longer supported in the activation. See "
      "google::api::expr::runtime::AttributeUtility.")
  virtual bool IsPathUnknown(absl::string_view) const { return false; }

  ABSL_DEPRECATED("Use missing_attribute_patterns() instead.")
  virtual const google::protobuf::FieldMask& unknown_paths() const {
    return google::protobuf::FieldMask::default_instance();
  }

  // Return the collection of attribute patterns that determine missing
  // attributes.
  virtual const std::vector<CelAttributePattern>& missing_attribute_patterns()
      const {
    static const std::vector<CelAttributePattern>* empty =
        new std::vector<CelAttributePattern>({});
    return *empty;
  }

  // Return the collection of attribute patterns that determine "unknown"
  // values.
  virtual const std::vector<CelAttributePattern>& unknown_attribute_patterns()
      const {
    static const std::vector<CelAttributePattern>* empty =
        new std::vector<CelAttributePattern>({});
    return *empty;
  }

  virtual ~BaseActivation() {}
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BASE_ACTIVATION_H_
