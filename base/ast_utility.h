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

#ifndef THIRD_PARTY_CEL_CPP_BASE_AST_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_BASE_AST_UTILITY_H_

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "base/ast.h"

namespace cel {
namespace ast {
namespace internal {

// Utilities for converting protobuf CEL message types to their corresponding
// internal C++ representations.
absl::StatusOr<Expr> ToNative(const google::api::expr::v1alpha1::Expr& expr);
absl::StatusOr<SourceInfo> ToNative(
    const google::api::expr::v1alpha1::SourceInfo& source_info);
absl::StatusOr<ParsedExpr> ToNative(
    const google::api::expr::v1alpha1::ParsedExpr& parsed_expr);
absl::StatusOr<Type> ToNative(const google::api::expr::v1alpha1::Type& type);
absl::StatusOr<Reference> ToNative(
    const google::api::expr::v1alpha1::Reference& reference);
absl::StatusOr<CheckedExpr> ToNative(
    const google::api::expr::v1alpha1::CheckedExpr& checked_expr);

// Conversion utility for the protobuf constant CEL value representation.
absl::StatusOr<Constant> ConvertConstant(
    const google::api::expr::v1alpha1::Constant& constant);

}  // namespace internal
}  // namespace ast
}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_AST_UTILITY_H_
