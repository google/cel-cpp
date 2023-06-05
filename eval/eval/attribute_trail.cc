#include "eval/eval/attribute_trail.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "eval/public/cel_attribute.h"

namespace google::api::expr::runtime {

// Creates AttributeTrail with attribute path incremented by "qualifier".
AttributeTrail AttributeTrail::Step(CelAttributeQualifier qualifier) const {
  // Cannot continue void trail
  if (empty()) return AttributeTrail();

  std::vector<CelAttributeQualifier> qualifiers;
  qualifiers.reserve(attribute_->qualifier_path().size() + 1);
  std::copy_n(attribute_->qualifier_path().begin(),
              attribute_->qualifier_path().size(),
              std::back_inserter(qualifiers));
  qualifiers.push_back(std::move(qualifier));
  return AttributeTrail(CelAttribute(std::string(attribute_->variable_name()),
                                     std::move(qualifiers)));
}

}  // namespace google::api::expr::runtime
