#include "eval/eval/attribute_utility.h"

#include "absl/status/status.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"

namespace google::api::expr::runtime {

using ::google::protobuf::Arena;

bool AttributeUtility::CheckForMissingAttribute(
    const AttributeTrail& trail) const {
  if (trail.empty()) {
    return false;
  }

  for (const auto& pattern : *missing_attribute_patterns_) {
    // (b/161297249) Preserving existing behavior for now, will add a streamz
    // for partial match, follow up with tightening up which fields are exposed
    // to the condition (w/ ajay and jim)
    if (pattern.IsMatch(*trail.attribute()) ==
        CelAttributePattern::MatchType::FULL) {
      return true;
    }
  }
  return false;
}

// Checks whether particular corresponds to any patterns that define unknowns.
bool AttributeUtility::CheckForUnknown(const AttributeTrail& trail,
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
const UnknownSet* AttributeUtility::MergeUnknowns(
    absl::Span<const CelValue> args, const UnknownSet* initial_set) const {
  const UnknownSet* result = initial_set;

  for (const auto& value : args) {
    if (!value.IsUnknownSet()) continue;

    auto current_set = value.UnknownSetOrDie();
    if (result == nullptr) {
      result = current_set;
    } else {
      result = memory_manager_.New<UnknownSet>(*result, *current_set).release();
    }
  }

  return result;
}

// Creates merged UnknownAttributeSet.
// Scans over the args collection, determines if there matches to unknown
// patterns, merges attributes together with those from initial_set
// (if initial_set is not null).
// Returns pointer to merged set or nullptr, if there were no sets to merge.
UnknownAttributeSet AttributeUtility::CheckForUnknowns(
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
const UnknownSet* AttributeUtility::MergeUnknowns(
    absl::Span<const CelValue> args, absl::Span<const AttributeTrail> attrs,
    const UnknownSet* initial_set, bool use_partial) const {
  UnknownAttributeSet attr_set = CheckForUnknowns(attrs, use_partial);
  if (!attr_set.attributes().empty()) {
    if (initial_set != nullptr) {
      initial_set =
          memory_manager_.New<UnknownSet>(*initial_set, UnknownSet(attr_set))
              .release();
    } else {
      initial_set = memory_manager_.New<UnknownSet>(attr_set).release();
    }
  }
  return MergeUnknowns(args, initial_set);
}
}  // namespace google::api::expr::runtime
