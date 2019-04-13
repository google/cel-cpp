/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_BASE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_BASE_H_

#include "eval/public/ast_visitor.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

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
  void PostVisitConst(const google::api::expr::v1alpha1::Constant* const_expr,
                      const google::api::expr::v1alpha1::Expr* expr,
                      const SourcePosition* position) override {}

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const google::api::expr::v1alpha1::Expr::Ident* ident_expr,
                      const google::api::expr::v1alpha1::Expr* expr,
                      const SourcePosition* position) override {}

  // Select node handler
  // Invoked after child nodes are processed.
  void PostVisitSelect(const google::api::expr::v1alpha1::Expr::Select* select_expr,
                       const google::api::expr::v1alpha1::Expr* expr,
                       const SourcePosition* position) override {}

  // Call node handler group
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const google::api::expr::v1alpha1::Expr::Call* call_expr,
                    const google::api::expr::v1alpha1::Expr* expr,
                    const SourcePosition* position) override {}

  // Invoked after all child nodes are processed.
  void PostVisitCall(const google::api::expr::v1alpha1::Expr::Call* call_expr,
                     const google::api::expr::v1alpha1::Expr* expr,
                     const SourcePosition* position) override {}

  // Invoked before all child nodes are processed.
  void PreVisitComprehension(
      const google::api::expr::v1alpha1::Expr::Comprehension* comprehension_expr,
      const google::api::expr::v1alpha1::Expr* expr,
      const SourcePosition* position) override {}

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(
      const google::api::expr::v1alpha1::Expr::Comprehension* comprehension_expr,
      const google::api::expr::v1alpha1::Expr* expr,
      const SourcePosition* position) override {}

  // Invoked after each argument node processed.
  // For Call arg_num is the index of the argument.
  // For Comprehension arg_num is specified by ComprehensionArg.
  void PostVisitArg(int arg_num, const google::api::expr::v1alpha1::Expr* expr,
                    const SourcePosition* position) override {}

  // CreateList node handler
  // Invoked after child nodes are processed.
  void PostVisitCreateList(const google::api::expr::v1alpha1::Expr::CreateList* list_expr,
                           const google::api::expr::v1alpha1::Expr* expr,
                           const SourcePosition* position) override {}

  // CreateStruct node handler
  // Invoked after child nodes are processed.
  void PostVisitCreateStruct(
      const google::api::expr::v1alpha1::Expr::CreateStruct* struct_expr,
      const google::api::expr::v1alpha1::Expr* expr,
      const SourcePosition* position) override {}
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_BASE_H_
