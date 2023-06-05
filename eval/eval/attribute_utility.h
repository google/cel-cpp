#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_

#include "absl/types/span.h"
#include "base/function_descriptor.h"
#include "base/function_result_set.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/value.h"
#include "eval/eval/attribute_trail.h"
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
      absl::Span<const cel::AttributePattern> unknown_patterns,
      absl::Span<const cel::AttributePattern> missing_attribute_patterns,
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
  const UnknownSet* MergeUnknowns(
      absl::Span<const cel::Handle<cel::Value>> args,
      const UnknownSet* initial_set) const;

  // Creates merged UnknownSet.
  // Merges together attributes from UnknownSets found in the args
  // collection, attributes from attr that match unknown pattern
  // patterns, and attributes from initial_set
  // (if initial_set is not null).
  // Returns pointer to merged set or nullptr, if there were no sets to merge.
  const UnknownSet* MergeUnknowns(
      absl::Span<const cel::Handle<cel::Value>> args,
      absl::Span<const AttributeTrail> attrs, const UnknownSet* initial_set,
      bool use_partial) const;

  // Create an initial UnknownSet from a single attribute.
  const UnknownSet* CreateUnknownSet(cel::Attribute attr) const;

  // Create an initial UnknownSet from a single missing function call.
  const UnknownSet* CreateUnknownSet(
      const cel::FunctionDescriptor& fn_descriptor, int64_t expr_id,
      absl::Span<const cel::Handle<cel::Value>> args) const;

 private:
  absl::Span<const cel::AttributePattern> unknown_patterns_;
  absl::Span<const cel::AttributePattern> missing_attribute_patterns_;
  cel::MemoryManager& memory_manager_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
