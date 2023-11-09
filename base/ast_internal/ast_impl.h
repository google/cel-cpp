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

#ifndef THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_AST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_AST_IMPL_H_

#include <cstdint>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "base/ast.h"
#include "base/ast_internal/expr.h"
#include "internal/casts.h"

namespace cel::ast_internal {

// Runtime implementation of a CEL abstract syntax tree.
// CEL users should not use this directly.
// If AST inspection is needed, prefer to use an existing tool or traverse the
// the protobuf representation.
class AstImpl : public Ast {
 public:
  // Overloads for down casting from the public interface to the internal
  // implementation.
  static AstImpl& CastFromPublicAst(Ast& ast) {
    return cel::internal::down_cast<AstImpl&>(ast);
  }

  static const AstImpl& CastFromPublicAst(const Ast& ast) {
    return cel::internal::down_cast<const AstImpl&>(ast);
  }

  static AstImpl* CastFromPublicAst(Ast* ast) {
    return cel::internal::down_cast<AstImpl*>(ast);
  }

  static const AstImpl* CastFromPublicAst(const Ast* ast) {
    return cel::internal::down_cast<const AstImpl*>(ast);
  }

  // Move-only
  AstImpl(const AstImpl& other) = delete;
  AstImpl& operator=(const AstImpl& other) = delete;
  AstImpl(AstImpl&& other) = default;
  AstImpl& operator=(AstImpl&& other) = default;

  explicit AstImpl(Expr expr, SourceInfo source_info)
      : root_expr_(std::move(expr)),
        source_info_(std::move(source_info)),
        is_checked_(false) {}

  explicit AstImpl(ParsedExpr expr)
      : root_expr_(std::move(expr.mutable_expr())),
        source_info_(std::move(expr.mutable_source_info())),
        is_checked_(false) {}

  explicit AstImpl(CheckedExpr expr)
      : root_expr_(std::move(expr.mutable_expr())),
        source_info_(std::move(expr.mutable_source_info())),
        reference_map_(std::move(expr.mutable_reference_map())),
        type_map_(std::move(expr.mutable_type_map())),
        is_checked_(true) {}

  // Implement public Ast APIs.
  bool IsChecked() const override { return is_checked_; }

  // Private functions.
  const Expr& root_expr() const { return root_expr_; }
  Expr& root_expr() { return root_expr_; }

  const SourceInfo& source_info() const { return source_info_; }
  SourceInfo& source_info() { return source_info_; }

  const Type& GetType(int64_t expr_id) const;
  const Type& GetReturnType() const;
  const Reference* GetReference(int64_t expr_id) const;

  const absl::flat_hash_map<int64_t, Reference>& reference_map() const {
    return reference_map_;
  }

  absl::flat_hash_map<int64_t, Reference>& reference_map() {
    return reference_map_;
  }

  const absl::flat_hash_map<int64_t, Type>& type_map() const {
    return type_map_;
  }

  AstImpl DeepCopy() const;

 private:
  Expr root_expr_;
  SourceInfo source_info_;
  absl::flat_hash_map<int64_t, Reference> reference_map_;
  absl::flat_hash_map<int64_t, Type> type_map_;
  bool is_checked_;
};

}  // namespace cel::ast_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_AST_IMPL_H_
