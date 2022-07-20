/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_NATIVE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_NATIVE_H_

#include "base/ast.h"
#include "eval/public/source_position_native.h"

namespace cel {
namespace ast {
namespace internal {

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

  // Expr node handler method. Called for all Expr nodes.
  // Is invoked before child Expr nodes being processed.
  virtual void PreVisitExpr(const Expr*, const SourcePosition*) = 0;

  // Expr node handler method. Called for all Expr nodes.
  // Is invoked after child Expr nodes are processed.
  virtual void PostVisitExpr(const Expr*, const SourcePosition*) = 0;

  // Const node handler.
  // Invoked after child nodes are processed.
  virtual void PostVisitConst(const Constant*, const Expr*,
                              const SourcePosition*) = 0;

  // Ident node handler.
  // Invoked after child nodes are processed.
  virtual void PostVisitIdent(const Ident*, const Expr*,
                              const SourcePosition*) = 0;

  // Select node handler
  // Invoked before child nodes are processed.
  virtual void PreVisitSelect(const Select*, const Expr*,
                              const SourcePosition*) = 0;

  // Select node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitSelect(const Select*, const Expr*,
                               const SourcePosition*) = 0;

  // Call node handler group
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  virtual void PreVisitCall(const Call*, const Expr*,
                            const SourcePosition*) = 0;

  // Invoked after all child nodes are processed.
  virtual void PostVisitCall(const Call*, const Expr*,
                             const SourcePosition*) = 0;

  // Invoked after target node is processed.
  // Expr is the call expression.
  virtual void PostVisitTarget(const Expr*, const SourcePosition*) = 0;

  // Invoked before all child nodes are processed.
  virtual void PreVisitComprehension(const Comprehension*, const Expr*,
                                     const SourcePosition*) = 0;

  // Invoked before comprehension child node is processed.
  virtual void PreVisitComprehensionSubexpression(
      const Expr* subexpr, const Comprehension* compr,
      ComprehensionArg comprehension_arg, const SourcePosition*) {}

  // Invoked after comprehension child node is processed.
  virtual void PostVisitComprehensionSubexpression(
      const Expr* subexpr, const Comprehension* compr,
      ComprehensionArg comprehension_arg, const SourcePosition*) {}

  // Invoked after all child nodes are processed.
  virtual void PostVisitComprehension(const Comprehension*, const Expr*,
                                      const SourcePosition*) = 0;

  // Invoked after each argument node processed.
  // For Call arg_num is the index of the argument.
  // For Comprehension arg_num is specified by ComprehensionArg.
  // Expr is the call expression.
  virtual void PostVisitArg(int arg_num, const Expr*,
                            const SourcePosition*) = 0;

  // CreateList node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitCreateList(const CreateList*, const Expr*,
                                   const SourcePosition*) = 0;

  // CreateStruct node handler
  // Invoked after child nodes are processed.
  virtual void PostVisitCreateStruct(const CreateStruct*, const Expr*,
                                     const SourcePosition*) = 0;
};

}  // namespace internal
}  // namespace ast
}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_VISITOR_H_
