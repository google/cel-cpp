// Copyright 2023 Google LLC
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
//
// API definitions for planner extensions.
//
// These are provided to indirect build dependencies for optional features and
// require detailed understanding of how the flat expression builder works and
// its assumptions.
//
// These interfaces should not be implemented directly by CEL users.
#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_EXTENSIONS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_EXTENSIONS_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "base/ast.h"
#include "base/ast_internal.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/public/cel_type_registry.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// Class representing FlatExpr internals exposed to extensions.
class PlannerContext {
 public:
  struct ProgramInfo {
    int range_start;
    int range_len = -1;
    const cel::ast::internal::Expr* parent = nullptr;
    std::vector<const cel::ast::internal::Expr*> children;
  };

  using ProgramTree =
      absl::flat_hash_map<const cel::ast::internal::Expr*, ProgramInfo>;

  explicit PlannerContext(const Resolver& resolver,
                          const CelTypeRegistry& type_registry,
                          const cel::RuntimeOptions& options,
                          BuilderWarnings& builder_warnings,
                          ExecutionPath& execution_path,
                          ProgramTree& program_tree)
      : resolver_(resolver),
        type_registry_(type_registry),
        options_(options),
        builder_warnings_(builder_warnings),
        execution_path_(execution_path),
        program_tree_(program_tree) {}

  // Note: this is invalidated after a sibling or parent is updated.
  ExecutionPathView GetSubplan(const cel::ast::internal::Expr& node) const;

  // Note: this can only safely be called on the node being visited.
  absl::Status ReplaceSubplan(const cel::ast::internal::Expr& node,
                              ExecutionPath path);

  const Resolver& resolver() const { return resolver_; }
  const CelTypeRegistry& type_registry() const { return type_registry_; }
  const cel::RuntimeOptions& options() const { return options_; }
  BuilderWarnings& builder_warnings() { return builder_warnings_; }

 private:
  const Resolver& resolver_;
  const CelTypeRegistry& type_registry_;
  const cel::RuntimeOptions& options_;
  BuilderWarnings& builder_warnings_;
  ExecutionPath& execution_path_;
  ProgramTree& program_tree_;
};

// Interface for Ast Transforms.
// If any are present, the flat expr builder will apply the Ast Transforms in
// order on a copy of the relevant input expressions before planning the
// program.
class AstTransform {
 public:
  virtual ~AstTransform() = default;

  virtual absl::Status UpdateAst(PlannerContext& context,
                                 cel::ast::internal::AstImpl& ast) const = 0;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_EXTENSIONS_H_
