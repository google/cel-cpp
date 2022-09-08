#include "eval/eval/attribute_utility.h"

#include <utility>

#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"

namespace google::api::expr::runtime {

bool AttributeUtility::CheckForMissingAttribute(
    const AttributeTrail& trail) const {
  if (trail.empty()) {
    return false;
  }

  for (const auto& pattern : *missing_attribute_patterns_) {
    // (b/161297249) Preserving existing behavior for now, will add a streamz
    // for partial match, follow up with tightening up which fields are exposed
    // to the condition (w/ ajay and jim)
    if (pattern.IsMatch(trail.attribute()) ==
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
    auto current_match = pattern.IsMatch(trail.attribute());
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
  absl::optional<UnknownSet> result_set;

  for (const auto& value : args) {
    if (!value.IsUnknownSet()) continue;

    auto current_set = value.UnknownSetOrDie();
    if (!result_set.has_value()) {
      if (initial_set != nullptr) {
        result_set.emplace(*initial_set);
      } else {
        result_set.emplace();
      }
    }
    result_set->Add(*current_set);
  }

  if (!result_set.has_value()) {
    return initial_set;
  }

  return memory_manager_.New<UnknownSet>(std::move(result_set).value())
      .release();
}

// Creates merged UnknownAttributeSet.
// Scans over the args collection, determines if there matches to unknown
// patterns, merges attributes together with those from initial_set
// (if initial_set is not null).
// Returns pointer to merged set or nullptr, if there were no sets to merge.
UnknownAttributeSet AttributeUtility::CheckForUnknowns(
    absl::Span<const AttributeTrail> args, bool use_partial) const {
  UnknownAttributeSet attribute_set;

  for (const auto& trail : args) {
    if (CheckForUnknown(trail, use_partial)) {
      attribute_set.Add(trail.attribute());
    }
  }

  return attribute_set;
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
  if (!attr_set.empty()) {
    UnknownSet result_set(std::move(attr_set));
    if (initial_set != nullptr) {
      result_set.Add(*initial_set);
    }
    for (const auto& value : args) {
      if (!value.IsUnknownSet()) {
        continue;
      }
      result_set.Add(*value.UnknownSetOrDie());
    }
    return memory_manager_.New<UnknownSet>(std::move(result_set)).release();
  }
  return MergeUnknowns(args, initial_set);
}
}  // namespace google::api::expr::runtime
