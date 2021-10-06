#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_

#include <vector>

#include "google/protobuf/arena.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "eval/eval/attribute_trail.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"

namespace google::api::expr::runtime {

// Helper class for handling unknowns and missing attribute logic. Provides
// helpers for merging unknown sets from arguments on the stack and for
// identifying unknown/missing attributes based on the patterns for a given
// Evaluation.
class AttributeUtility {
 public:
  AttributeUtility(
      const std::vector<CelAttributePattern>* unknown_patterns,
      const std::vector<CelAttributePattern>* missing_attribute_patterns,
      google::protobuf::Arena* arena)
      : unknown_patterns_(unknown_patterns),
        missing_attribute_patterns_(missing_attribute_patterns),
        arena_(arena) {}

  // Checks whether particular corresponds to any patterns that define missing
  // attribute.
  bool CheckForMissingAttribute(const AttributeTrail& trail) const;

  // Checks whether particular corresponds to any patterns that define unknowns.
  bool CheckForUnknown(const AttributeTrail& trail, bool use_partial) const;

  // Creates merged UnknownAttributeSet.
  // Scans over the args collection, determines if there matches to unknown
  // patterns and returns the (possibly empty) collection.
  UnknownAttributeSet CheckForUnknowns(absl::Span<const AttributeTrail> args,
                                       bool use_partial) const;

  // Creates merged UnknownSet.
  // Scans over the args collection, merges any UnknownAttributeSets found in
  // it together with initial_set (if initial_set is not null).
  // Returns pointer to merged set or nullptr, if there were no sets to merge.
  const UnknownSet* MergeUnknowns(absl::Span<const CelValue> args,
                                  const UnknownSet* initial_set) const;

  // Creates merged UnknownSet.
  // Merges together attributes from UnknownSets found in the args
  // collection, attributes from attr that match unknown pattern
  // patterns, and attributes from initial_set
  // (if initial_set is not null).
  // Returns pointer to merged set or nullptr, if there were no sets to merge.
  const UnknownSet* MergeUnknowns(absl::Span<const CelValue> args,
                                  absl::Span<const AttributeTrail> attrs,
                                  const UnknownSet* initial_set,
                                  bool use_partial) const;

 private:
  const std::vector<CelAttributePattern>* unknown_patterns_;
  const std::vector<CelAttributePattern>* missing_attribute_patterns_;
  google::protobuf::Arena* arena_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
