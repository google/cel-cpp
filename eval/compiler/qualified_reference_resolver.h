#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_

#include <cstdint>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/expression_build_warning.h"

namespace google::api::expr::runtime {

// Resolves possibly qualified names in the provided expression, updating
// subexpressions with to use the fully qualified name, or a constant
// expressions in the case of enums.
//
// Returns true if updates were applied.
//
// Will warn or return a non-ok status if references can't be resolved (no
// function overload could match a call) or are inconsistnet (reference map
// points to an expr node that isn't a reference).
absl::StatusOr<bool> ResolveReferences(
    const google::protobuf::Map<int64_t, google::api::expr::v1alpha1::Reference>* reference_map,
    const Resolver& resolver, const google::api::expr::v1alpha1::SourceInfo* source_info,
    BuilderWarnings& warnings, google::api::expr::v1alpha1::Expr* expr);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
