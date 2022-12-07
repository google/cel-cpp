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

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "base/ast_internal.h"

namespace cel::extensions::internal {

// Utilities for converting protobuf CEL message types to their corresponding
// internal C++ representations.
absl::StatusOr<ast::internal::Expr> ConvertProtoExprToNative(
    const google::api::expr::v1alpha1::Expr& expr);
absl::StatusOr<ast::internal::SourceInfo> ConvertProtoSourceInfoToNative(
    const google::api::expr::v1alpha1::SourceInfo& source_info);
absl::StatusOr<ast::internal::ParsedExpr> ConvertProtoParsedExprToNative(
    const google::api::expr::v1alpha1::ParsedExpr& parsed_expr);
absl::StatusOr<ast::internal::Type> ConvertProtoTypeToNative(
    const google::api::expr::v1alpha1::Type& type);
absl::StatusOr<ast::internal::Reference> ConvertProtoReferenceToNative(
    const google::api::expr::v1alpha1::Reference& reference);
absl::StatusOr<ast::internal::CheckedExpr> ConvertProtoCheckedExprToNative(
    const google::api::expr::v1alpha1::CheckedExpr& checked_expr);

// Conversion utility for the protobuf constant CEL value representation.
absl::StatusOr<ast::internal::Constant> ConvertConstant(
    const google::api::expr::v1alpha1::Constant& constant);

}  // namespace cel::extensions::internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_AST_CONVERTERS_H_
