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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_

#include "eval/public/ast_visitor.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// This method performs traversal of AST.
// expr is root node of the tree.
// handler is callback object.
void AstTraverse(const google::api::expr::v1alpha1::Expr *expr,
                 const google::api::expr::v1alpha1::SourceInfo *source_info,
                 AstVisitor *visitor);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_AST_TRAVERSE_H_
