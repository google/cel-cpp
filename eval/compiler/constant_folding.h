#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "base/ast_internal.h"
#include "base/value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "runtime/function_registry.h"
#include "google/protobuf/arena.h"

namespace cel::ast::internal {

// A transformation over input expression that produces a new expression with
// constant sub-expressions replaced by generated idents in the constant_idents
// map. This transformation preserves the IDs of the input sub-expressions.
void FoldConstants(
    const Expr& ast, const FunctionRegistry& registry, google::protobuf::Arena* arena,
    absl::flat_hash_map<std::string, Handle<Value>>& constant_idents,
    Expr& out_ast);

// Create a new constant folding extension.
// Eagerly evaluates sub expressions with all constant inputs, and replaces said
// sub expression with the result.
google::api::expr::runtime::ProgramOptimizerFactory
CreateConstantFoldingExtension(google::protobuf::Arena* arena);

}  // namespace cel::ast::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
