#include "eval/eval/unknowns_utility.h"

#include "absl/status/status.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "base/statusor.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::protobuf::Arena;

// Checks whether particular corresponds to any patterns that define unknowns.
bool UnknownsUtility::CheckForUnknown(const AttributeTrail& trail,
                                      bool use_partial) const {
  if (trail.empty()) {
    return false;
  }
  for (const auto& pattern : *unknown_patterns_) {
    auto current_match = pattern.IsMatch(*trail.attribute());
    if (current_match == CelAttributePattern::MatchType::FULL ||
        (use_partial &&
         current_match == CelAttributePattern::MatchType::PARTIAL)) {
      return true;
    }
  }
  return false;
}

// Creates merged UnknownAttributeSet.
// Scans over the args collection, merges any UnknownSets found in
// it together with initial_set (if initial_set is not null).
// Returns pointer to merged set or nullptr, if there were no sets to merge.
const UnknownSet* UnknownsUtility::MergeUnknowns(
    absl::Span<const CelValue> args, const UnknownSet* initial_set) const {
  const UnknownSet* result = initial_set;

  for (const auto& value : args) {
    if (!value.IsUnknownSet()) continue;

    auto current_set = value.UnknownSetOrDie();
    if (result == nullptr) {
      result = current_set;
    } else {
      result = Arena::Create<UnknownSet>(arena_, *result, *current_set);
    }
  }

  return result;
}

// Creates merged UnknownAttributeSet.
// Scans over the args collection, determines if there matches to unknown
// patterns, merges attributes together with those from initial_set
// (if initial_set is not null).
// Returns pointer to merged set or nullptr, if there were no sets to merge.
UnknownAttributeSet UnknownsUtility::CheckForUnknowns(
    absl::Span<const AttributeTrail> args, bool use_partial) const {
  std::vector<const CelAttribute*> unknown_attrs;

  for (auto trail : args) {
    if (CheckForUnknown(trail, use_partial)) {
      unknown_attrs.push_back(trail.attribute());
    }
  }

  return UnknownAttributeSet(unknown_attrs);
}

// Creates merged UnknownAttributeSet.
// Merges together attributes from UnknownAttributeSets found in the args
// collection, attributes from attr that match unknown pattern
// patterns, and attributes from initial_set
// (if initial_set is not null).
// Returns pointer to merged set or nullptr, if there were no sets to merge.
const UnknownSet* UnknownsUtility::MergeUnknowns(
    absl::Span<const CelValue> args, absl::Span<const AttributeTrail> attrs,
    const UnknownSet* initial_set, bool use_partial) const {
  UnknownAttributeSet attr_set = CheckForUnknowns(attrs, use_partial);
  if (!attr_set.attributes().empty()) {
    if (initial_set != nullptr) {
      initial_set =
          Arena::Create<UnknownSet>(arena_, *initial_set, UnknownSet(attr_set));
    } else {
      initial_set = Arena::Create<UnknownSet>(arena_, attr_set);
    }
  }
  return MergeUnknowns(args, initial_set);
}
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
