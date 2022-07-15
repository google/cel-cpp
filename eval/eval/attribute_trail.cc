#include "eval/eval/attribute_trail.h"

#include <utility>

#include "absl/status/status.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

AttributeTrail::AttributeTrail(google::api::expr::v1alpha1::Expr root,
                               cel::MemoryManager& manager) {
  attribute_ = manager
                   .New<CelAttribute>(std::move(root),
                                      std::vector<CelAttributeQualifier>())
                   .release();
}

// Creates AttributeTrail with attribute path incremented by "qualifier".
AttributeTrail AttributeTrail::Step(CelAttributeQualifier qualifier,
                                    cel::MemoryManager& manager) const {
  // Cannot continue void trail
  if (empty()) return AttributeTrail();

  std::vector<CelAttributeQualifier> qualifiers = attribute_->qualifier_path();
  qualifiers.push_back(qualifier);
  auto attribute = manager.New<CelAttribute>(
      std::string(attribute_->variable_name()), std::move(qualifiers));
  return AttributeTrail(attribute.release());
}

}  // namespace google::api::expr::runtime
