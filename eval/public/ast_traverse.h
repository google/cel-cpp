#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_

#include "eval/public/ast_visitor.h"
#include "syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// This method performs traversal of AST.
// expr is root node of the tree.
// handler is callback object.
void AstTraverse(const google::api::expr::v1::Expr *expr,
                 const google::api::expr::v1::SourceInfo *source_info,
                 AstVisitor *visitor);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_
