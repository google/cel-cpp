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

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "base/ast.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/value_factory.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/evaluator_core.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// Class representing FlatExpr internals exposed to extensions.
class PlannerContext {
 public:
  struct ProgramInfo {
    int range_start;
    int range_len = -1;
    const cel::ast_internal::Expr* parent = nullptr;
    std::vector<const cel::ast_internal::Expr*> children;
  };

  using ProgramTree =
      absl::flat_hash_map<const cel::ast_internal::Expr*, ProgramInfo>;

  explicit PlannerContext(
      const Resolver& resolver, const cel::RuntimeOptions& options,
      cel::ValueFactory& value_factory,
      cel::runtime_internal::IssueCollector& issue_collector,
      ExecutionPath& execution_path, ProgramTree& program_tree)
      : resolver_(resolver),
        value_factory_(value_factory),
        options_(options),
        issue_collector_(issue_collector),
        execution_path_(execution_path),
        program_tree_(program_tree) {}

  // Note: this is invalidated after a sibling or parent is updated.
  ExecutionPathView GetSubplan(const cel::ast_internal::Expr& node) const;

  // Extract the plan steps for the given expr.
  // The backing execution path is not resized -- a later call must
  // overwrite the extracted region.
  absl::StatusOr<ExecutionPath> ExtractSubplan(
      const cel::ast_internal::Expr& node);

  // Note: this can only safely be called on the node being visited.
  absl::Status ReplaceSubplan(const cel::ast_internal::Expr& node,
                              ExecutionPath path);

  const Resolver& resolver() const { return resolver_; }
  cel::ValueFactory& value_factory() const { return value_factory_; }
  const cel::RuntimeOptions& options() const { return options_; }
  cel::runtime_internal::IssueCollector& issue_collector() {
    return issue_collector_;
  }

 private:
  const Resolver& resolver_;
  cel::ValueFactory& value_factory_;
  const cel::RuntimeOptions& options_;
  cel::runtime_internal::IssueCollector& issue_collector_;
  ExecutionPath& execution_path_;
  ProgramTree& program_tree_;
};

// Interface for Ast Transforms.
// If any are present, the FlatExprBuilder will apply the Ast Transforms in
// order on a copy of the relevant input expressions before planning the
// program.
class AstTransform {
 public:
  virtual ~AstTransform() = default;

  virtual absl::Status UpdateAst(PlannerContext& context,
                                 cel::ast_internal::AstImpl& ast) const = 0;
};

// Interface for program optimizers.
//
// If any are present, the FlatExprBuilder will notify the implementations in
// order as it traverses the input ast.
//
// Note: implementations must correctly check that subprograms are available
// before accessing (i.e. they have not already been edited).
class ProgramOptimizer {
 public:
  virtual ~ProgramOptimizer() = default;

  // Called before planning the given expr node.
  virtual absl::Status OnPreVisit(PlannerContext& context,
                                  const cel::ast_internal::Expr& node) = 0;

  // Called after planning the given expr node.
  virtual absl::Status OnPostVisit(PlannerContext& context,
                                   const cel::ast_internal::Expr& node) = 0;
};

// Type definition for ProgramOptimizer factories.
//
// The expression builder must remain thread compatible, but ProgramOptimizers
// are often stateful for a given expression. To avoid requiring the optimizer
// implementation to handle concurrent planning, the builder creates a new
// instance per expression planned.
//
// The factory must be thread safe, but the returned instance may assume
// it is called from a synchronous context.
using ProgramOptimizerFactory =
    absl::AnyInvocable<absl::StatusOr<std::unique_ptr<ProgramOptimizer>>(
        PlannerContext&, const cel::ast_internal::AstImpl&) const>;

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_EXTENSIONS_H_
