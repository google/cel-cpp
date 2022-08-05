#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_

#include <memory>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "eval/public/cel_attribute.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

class AttributeUtility;
class UnknownSet;

// UnknownAttributeSet is a container for CEL attributes that are identified as
// unknown during expression evaluation.
class UnknownAttributeSet final {
 private:
  using Container = absl::btree_set<CelAttribute>;

 public:
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using iterator = typename Container::const_iterator;
  using const_iterator = typename Container::const_iterator;

  UnknownAttributeSet() = default;
  UnknownAttributeSet(const UnknownAttributeSet&) = default;
  UnknownAttributeSet(UnknownAttributeSet&&) = default;
  UnknownAttributeSet& operator=(const UnknownAttributeSet&) = default;
  UnknownAttributeSet& operator=(UnknownAttributeSet&&) = default;

  explicit UnknownAttributeSet(const std::vector<CelAttribute>& attributes) {
    for (const auto& attr : attributes) {
      Add(attr);
    }
  }

  UnknownAttributeSet(const UnknownAttributeSet& set1,
                      const UnknownAttributeSet& set2)
      : attributes_(set1.attributes_) {
    for (const auto& attr : set2.attributes_) {
      Add(attr);
    }
  }

  iterator begin() const { return attributes_.begin(); }

  const_iterator cbegin() const { return attributes_.cbegin(); }

  iterator end() const { return attributes_.end(); }

  const_iterator cend() const { return attributes_.cend(); }

  size_type size() const { return attributes_.size(); }

  bool empty() const { return attributes_.empty(); }

  bool operator==(const UnknownAttributeSet& other) const {
    return this == &other || attributes_ == other.attributes_;
  }

  bool operator!=(const UnknownAttributeSet& other) const {
    return !operator==(other);
  }

  static UnknownAttributeSet Merge(const UnknownAttributeSet& set1,
                                   const UnknownAttributeSet& set2) {
    return UnknownAttributeSet(set1, set2);
  }

 private:
  friend class AttributeUtility;
  friend class UnknownSet;

  void Add(const CelAttribute& attribute) { attributes_.insert(attribute); }

  void Add(const UnknownAttributeSet& other) {
    for (const auto& attribute : other) {
      Add(attribute);
    }
  }

  // Attribute container.
  Container attributes_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_
