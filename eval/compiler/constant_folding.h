#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "base/ast_internal.h"
#include "base/value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expression.h"
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

class ConstantFoldingExtension {
 public:
  ConstantFoldingExtension(int stack_limit, google::protobuf::Arena* arena)
      : arena_(arena), state_(stack_limit, arena) {}

  absl::Status OnPreVisit(google::api::expr::runtime::PlannerContext& context,
                          const Expr& node);
  absl::Status OnPostVisit(google::api::expr::runtime::PlannerContext& context,
                           const Expr& node);

 private:
  enum class IsConst {
    kConditional,
    kNonConst,
  };

  google::protobuf::Arena* arena_;
  google::api::expr::runtime::Activation empty_;
  google::api::expr::runtime::CelEvaluationListener null_listener_;
  google::api::expr::runtime::CelExpressionFlatEvaluationState state_;

  std::vector<IsConst> is_const_;
};

}  // namespace cel::ast::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
