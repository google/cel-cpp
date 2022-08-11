#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_

#include <cstdint>

#include "google/protobuf/map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/ast.h"
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
    const absl::flat_hash_map<int64_t, cel::ast::internal::Reference>*
        reference_map,
    const Resolver& resolver, const cel::ast::internal::SourceInfo* source_info,
    BuilderWarnings& warnings, cel::ast::internal::Expr* expr);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
