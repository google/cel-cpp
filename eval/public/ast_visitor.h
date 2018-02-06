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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_H_

#include "eval/public/source_position.h"
#include "syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// ComprehensionArg specifies arg_num values passed to PostVisitArg
// for subexpressions of Comprehension.
enum ComprehensionArg {
  ITER_RANGE,
  ACCU_INIT,
  LOOP_CONDITION,
  LOOP_STEP,
  RESULT,
};

// Callback handler class, used in conjunction with AstTraverse.
// Methods of this class are invoked when AST nodes with corresponding
// types are processed.
//
// For all types with children, the children will be visited in the natural
// order from first to last.  For structs, keys are visited before values.
class AstVisitor {
 public:
  virtual ~AstVisitor() {}

  // Const node handler.
  // Invoked after child nodes are processed.
  virtual void PostVisitConst(const google::api::expr::v1::Constant* const_expr,
                              const google::api::expr::v1::Expr* expr,
                              const SourcePosition* position) = 0;

  // Ident node handler.
  // Invoked after child nodes are processed.
  virtual void PostVisitIdent(const google::api::expr::v1::Expr::Ident* ident_expr,
                              const google::api::expr::v1::Expr* expr,
                              const SourcePosition* position) = 0;

  // Select node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitSelect(
      const google::api::expr::v1::Expr::Select* select_expr,
      const google::api::expr::v1::Expr* expr, const SourcePosition* position) = 0;

  // Call node handler group
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  virtual void PreVisitCall(const google::api::expr::v1::Expr::Call* call_expr,
                            const google::api::expr::v1::Expr* expr,
                            const SourcePosition* position) = 0;

  // Invoked after all child nodes are processed.
  virtual void PostVisitCall(const google::api::expr::v1::Expr::Call* call_expr,
                             const google::api::expr::v1::Expr* expr,
                             const SourcePosition* position) = 0;

  // Invoked before all child nodes are processed.
  virtual void PreVisitComprehension(
      const google::api::expr::v1::Expr::Comprehension* comprehension_expr,
      const google::api::expr::v1::Expr* expr, const SourcePosition* position) = 0;

  // Invoked after all child nodes are processed.
  virtual void PostVisitComprehension(
      const google::api::expr::v1::Expr::Comprehension* comprehension_expr,
      const google::api::expr::v1::Expr* expr, const SourcePosition* position) = 0;

  // Invoked after each argument node processed.
  // For Call arg_num is the index of the argument.
  // For Comprehension arg_num is specified by ComprehensionArg.
  virtual void PostVisitArg(int arg_num, const google::api::expr::v1::Expr* expr,
                            const SourcePosition* position) = 0;

  // CreateList node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitCreateList(
      const google::api::expr::v1::Expr::CreateList* list_expr,
      const google::api::expr::v1::Expr* expr, const SourcePosition* position) = 0;

  // CreateStruct node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitCreateStruct(
      const google::api::expr::v1::Expr::CreateStruct* struct_expr,
      const google::api::expr::v1::Expr* expr, const SourcePosition* position) = 0;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_H_
