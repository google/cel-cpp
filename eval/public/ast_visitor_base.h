#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_BASE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_BASE_H_

#include "eval/public/ast_visitor.h"
#include "syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Trivial base implementation of AstVisitor.
class AstVisitorBase : public AstVisitor {
 public:
  AstVisitorBase() {}

  // Non-copyable
  AstVisitorBase(const AstVisitorBase&) = delete;
  AstVisitorBase& operator=(AstVisitorBase const&) = delete;

  ~AstVisitorBase() override {}

  // Const node handler.
  // Invoked after child nodes are processed.
  void PostVisitConst(const google::api::expr::v1::Constant* const_expr,
                      const google::api::expr::v1::Expr* expr,
                      const SourcePosition* position) override {}

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const google::api::expr::v1::Expr::Ident* ident_expr,
                      const google::api::expr::v1::Expr* expr,
                      const SourcePosition* position) override {}

  // Select node handler
  // Invoked after child nodes are processed.
  void PostVisitSelect(const google::api::expr::v1::Expr::Select* select_expr,
                       const google::api::expr::v1::Expr* expr,
                       const SourcePosition* position) override {}

  // Call node handler group
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const google::api::expr::v1::Expr::Call* call_expr,
                    const google::api::expr::v1::Expr* expr,
                    const SourcePosition* position) override {}

  // Invoked after all child nodes are processed.
  void PostVisitCall(const google::api::expr::v1::Expr::Call* call_expr,
                     const google::api::expr::v1::Expr* expr,
                     const SourcePosition* position) override {}

  // Invoked before all child nodes are processed.
  void PreVisitComprehension(
      const google::api::expr::v1::Expr::Comprehension* comprehension_expr,
      const google::api::expr::v1::Expr* expr,
      const SourcePosition* position) override {}

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(
      const google::api::expr::v1::Expr::Comprehension* comprehension_expr,
      const google::api::expr::v1::Expr* expr,
      const SourcePosition* position) override {}

  // Invoked after each argument node processed.
  // For Call arg_num is the index of the argument.
  // For Comprehension arg_num is specified by ComprehensionArg.
  void PostVisitArg(int arg_num, const google::api::expr::v1::Expr* expr,
                    const SourcePosition* position) override {}

  // CreateList node handler
  // Invoked after child nodes are processed.
  void PostVisitCreateList(const google::api::expr::v1::Expr::CreateList* list_expr,
                           const google::api::expr::v1::Expr* expr,
                           const SourcePosition* position) override {}

  // CreateStruct node handler
  // Invoked after child nodes are processed.
  void PostVisitCreateStruct(
      const google::api::expr::v1::Expr::CreateStruct* struct_expr,
      const google::api::expr::v1::Expr* expr,
      const SourcePosition* position) override {}
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_BASE_H_
