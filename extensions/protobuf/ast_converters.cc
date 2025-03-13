// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "extensions/protobuf/ast_converters.h"

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/status/statusor.h"
#include "common/ast.h"
#include "common/ast_proto.h"
#include "internal/status_macros.h"

namespace cel::extensions {

using ::cel::expr::CheckedExpr;
using ::cel::expr::ParsedExpr;

absl::StatusOr<ParsedExpr> CreateParsedExprFromAst(const Ast& ast) {
  ParsedExpr parsed_expr;
  CEL_RETURN_IF_ERROR(AstToParsedExpr(ast, &parsed_expr));
  return parsed_expr;
}

absl::StatusOr<cel::expr::CheckedExpr> CreateCheckedExprFromAst(
    const Ast& ast) {
  CheckedExpr checked_expr;
  CEL_RETURN_IF_ERROR(AstToCheckedExpr(ast, &checked_expr));
  return checked_expr;
}

}  // namespace cel::extensions
