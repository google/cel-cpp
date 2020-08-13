#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_

#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/types/optional.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// AttributeTrail reflects current attribute path.
// It is functionally similar to CelAttribute, yet intended to have better
// complexity on attribute path increment operations.
// TODO(issues/41) Current AttributeTrail implementation is equivalent to
// CelAttribute - improve it.
// Intended to be used in conjunction with CelValue, describing the attribute
// value originated from.
// Empty AttributeTrail denotes object with attribute path not defined
// or supported.
class AttributeTrail {
 public:
  AttributeTrail() : attribute_(nullptr) {}
  AttributeTrail(google::api::expr::v1alpha1::Expr root, google::protobuf::Arena* arena)
      : AttributeTrail(google::protobuf::Arena::Create<CelAttribute>(
            arena, root, std::vector<CelAttributeQualifier>())) {}

  // Creates AttributeTrail with attribute path incremented by "qualifier".
  AttributeTrail Step(CelAttributeQualifier qualifier,
                      google::protobuf::Arena* arena) const;

  // Creates AttributeTrail with attribute path incremented by "qualifier".
  AttributeTrail Step(const std::string* qualifier,
                      google::protobuf::Arena* arena) const {
    return Step(
        CelAttributeQualifier::Create(CelValue::CreateString(qualifier)),
        arena);
  }

  // Returns CelAttribute that corresponds to content of AttributeTrail.
  const CelAttribute* attribute() const { return attribute_; }

  bool empty() const { return !attribute_; }

 private:
  AttributeTrail(const CelAttribute* attribute) : attribute_(attribute) {}
  const CelAttribute* attribute_;
};
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_
