#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_

#include "eval/compiler/flat_expr_builder_extensions.h"
#include "google/protobuf/arena.h"

namespace cel::ast::internal {

// Create a new constant folding extension.
// Eagerly evaluates sub expressions with all constant inputs, and replaces said
// sub expression with the result.
google::api::expr::runtime::ProgramOptimizerFactory
CreateConstantFoldingExtension(google::protobuf::Arena* arena);

}  // namespace cel::ast::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
