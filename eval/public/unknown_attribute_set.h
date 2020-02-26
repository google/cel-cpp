#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_

#include <memory>

#include "absl/container/flat_hash_set.h"
#include "eval/public/cel_attribute.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// UnknownAttributeSet is a container for CEL attributes that are identified as
// unknown during expression evaluation.
class UnknownAttributeSet {
 public:
  UnknownAttributeSet(const UnknownAttributeSet& other) = default;
  UnknownAttributeSet& operator=(const UnknownAttributeSet& other) = default;

  UnknownAttributeSet() {}
  UnknownAttributeSet(const std::vector<const CelAttribute*>& attributes) {
    attributes_.reserve(attributes.size());
    for (const auto& attr : attributes) {
      Add(attr);
    }
  }

  UnknownAttributeSet(const UnknownAttributeSet& set1,
                      const UnknownAttributeSet& set2)
      : attributes_(set1.attributes()) {
    attributes_.reserve(set1.attributes().size() + set2.attributes().size());
    for (const auto& attr : set2.attributes()) {
      Add(attr);
    }
  }

  std::vector<const CelAttribute*> attributes() const { return attributes_; }

  static UnknownAttributeSet Merge(const UnknownAttributeSet& set1,
                                   const UnknownAttributeSet& set2) {
    return UnknownAttributeSet(set1, set2);
  }

 private:
  void Add(const CelAttribute* attribute) {
    if (!attribute) {
      return;
    }
    for (auto attr : attributes_) {
      if (*attr == *attribute) {
        return;
      }
    }
    attributes_.push_back(attribute);
  }

  // Attribute container.
  std::vector<const CelAttribute*> attributes_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_
