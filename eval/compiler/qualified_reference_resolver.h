#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_

#include <memory>

#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/internal/ast_impl.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
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
absl::StatusOr<bool> ResolveReferences(const Resolver& resolver,
                                       BuilderWarnings& warnings,
                                       cel::ast::internal::AstImpl& ast);

enum class ReferenceResolverOption {
  // Always attempt to resolve references based on runtime types and functions.
  kAlways,
  // Only attempt to resolve for checked expressions with reference metadata.
  kCheckedOnly,
};

std::unique_ptr<AstTransform> NewReferenceResolverExtension(
    ReferenceResolverOption option);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
