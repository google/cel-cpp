#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/public/cel_function_registry.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// A transformation over input expression that produces a new expression with
// subexpressions replaced by appropriate expressions referring to the
// fully-qualified entity name or constant expressions in case of enums.
// Returns modified expr if updates found.
// Otherwise, returns nullopt.
absl::StatusOr<absl::optional<google::api::expr::v1alpha1::Expr>> ResolveReferences(
    const google::api::expr::v1alpha1::Expr& expr,
    const google::protobuf::Map<int64_t, google::api::expr::v1alpha1::Reference>& reference_map,
    const CelFunctionRegistry& registry, absl::string_view container,
    BuilderWarnings* warnings);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_QUALIFIED_REFERENCE_RESOLVER_H_
