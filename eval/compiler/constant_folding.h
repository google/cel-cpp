#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_

#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/flat_hash_map.h"
#include "base/ast.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_value.h"

namespace cel::ast::internal {

// A transformation over input expression that produces a new expression with
// constant sub-expressions replaced by generated idents in the constant_idents
// map. This transformation preserves the IDs of the input sub-expressions.
void FoldConstants(
    const Expr& expr,
    const google::api::expr::runtime::CelFunctionRegistry& registry,
    google::protobuf::Arena* arena,
    absl::flat_hash_map<std::string, google::api::expr::runtime::CelValue>&
        constant_idents,
    Expr* out);

}  // namespace cel::ast::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
