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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_AST_CONVERTERS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_AST_CONVERTERS_H_

#include <memory>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "common/ast/expr.h"

namespace cel::extensions {
namespace internal {

// Conversion utility for the CEL source info representation to the protobuf
// representation.
absl::StatusOr<cel::expr::SourceInfo> ConvertSourceInfoToProto(
    const ast_internal::SourceInfo& source_info);

}  // namespace internal

// Creates a runtime AST from a parsed-only protobuf AST.
// May return a non-ok Status if the AST is malformed (e.g. unset required
// fields).
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const cel::expr::Expr& expr,
    const cel::expr::SourceInfo* source_info = nullptr);
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const cel::expr::ParsedExpr& parsed_expr);

absl::StatusOr<cel::expr::ParsedExpr> CreateParsedExprFromAst(
    const Ast& ast);

// Creates a runtime AST from a checked protobuf AST.
// May return a non-ok Status if the AST is malformed (e.g. unset required
// fields).
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromCheckedExpr(
    const cel::expr::CheckedExpr& checked_expr);

absl::StatusOr<cel::expr::CheckedExpr> CreateCheckedExprFromAst(
    const Ast& ast);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_AST_CONVERTERS_H_
