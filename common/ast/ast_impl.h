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

#include <string>
#include <utility>

#include "common/ast.h"
#include "common/ast/expr.h"
#include "common/ast/metadata.h"  // IWYU pragma: export
#include "common/expr.h"

namespace cel::ast_internal {

// Trivial subclass of the public Ast.
//
// Temporarily needed to transition to just using the public Ast.
class AstImpl : public Ast {
 public:
  using ReferenceMap = Ast::ReferenceMap;
  using TypeMap = Ast::TypeMap;

  AstImpl() = default;

  AstImpl(Expr expr, SourceInfo source_info)
      : Ast(std::move(expr), std::move(source_info)) {}

  AstImpl(Expr expr, SourceInfo source_info, ReferenceMap reference_map,
          TypeMap type_map, std::string expr_version)
      : Ast(std::move(expr), std::move(source_info), std::move(reference_map),
            std::move(type_map), std::move(expr_version)) {}

  // Move-only
  AstImpl(const AstImpl& other) = delete;
  AstImpl& operator=(const AstImpl& other) = delete;
  AstImpl(AstImpl&& other) = default;
  AstImpl& operator=(AstImpl&& other) = default;
};

}  // namespace cel::ast_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_AST_IMPL_H_
