#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_

#include <utility>
#include <vector>

#include "google/protobuf/arena.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/memory_manager.h"
#include "eval/eval/attribute_trail.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_function_result_set.h"
#include "eval/public/unknown_set.h"

namespace google::api::expr::runtime {

// Helper class for handling unknowns and missing attribute logic. Provides
// helpers for merging unknown sets from arguments on the stack and for
// identifying unknown/missing attributes based on the patterns for a given
// Evaluation.
// Neither moveable nor copyable.
class AttributeUtility {
 public:
  AttributeUtility(
      const std::vector<CelAttributePattern>* unknown_patterns,
      const std::vector<CelAttributePattern>* missing_attribute_patterns,
      cel::MemoryManager& manager)
      : unknown_patterns_(unknown_patterns),
        missing_attribute_patterns_(missing_attribute_patterns),
        memory_manager_(manager) {}

  AttributeUtility(const AttributeUtility&) = delete;
  AttributeUtility& operator=(const AttributeUtility&) = delete;
  AttributeUtility(AttributeUtility&&) = delete;
  AttributeUtility& operator=(AttributeUtility&&) = delete;

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

  // Create an initial UnknownSet from a single attribute.
  const UnknownSet* CreateUnknownSet(CelAttribute attr) const {
    return memory_manager_
        .New<UnknownSet>(UnknownAttributeSet({std::move(attr)}))
        .release();
  }

  // Create an initial UnknownSet from a single missing function call.
  const UnknownSet* CreateUnknownSet(const CelFunctionDescriptor& fn_descriptor,
                                     int64_t expr_id,
                                     absl::Span<const CelValue> args) const {
    return memory_manager_
        .New<UnknownSet>(UnknownFunctionResultSet(
            UnknownFunctionResult(fn_descriptor, expr_id)))
        .release();
  }

 private:
  const std::vector<CelAttributePattern>* unknown_patterns_;
  const std::vector<CelAttributePattern>* missing_attribute_patterns_;
  cel::MemoryManager& memory_manager_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
