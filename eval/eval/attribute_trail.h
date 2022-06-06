#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_

#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/types/optional.h"
#include "base/memory_manager.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"

namespace google::api::expr::runtime {

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

  AttributeTrail(google::api::expr::v1alpha1::Expr root, cel::MemoryManager& manager);

  // Creates AttributeTrail with attribute path incremented by "qualifier".
  AttributeTrail Step(CelAttributeQualifier qualifier,
                      cel::MemoryManager& manager) const;

  // Creates AttributeTrail with attribute path incremented by "qualifier".
  AttributeTrail Step(const std::string* qualifier,
                      cel::MemoryManager& manager) const {
    return Step(
        CelAttributeQualifier::Create(CelValue::CreateString(qualifier)),
        manager);
  }

  // Returns CelAttribute that corresponds to content of AttributeTrail.
  const CelAttribute* attribute() const { return attribute_; }

  bool empty() const { return attribute_ == nullptr; }

 private:
  explicit AttributeTrail(const CelAttribute* attribute)
      : attribute_(attribute) {}
  const CelAttribute* attribute_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_
