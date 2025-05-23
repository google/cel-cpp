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

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/ast.h"
#include "base/type_provider.h"
#include "common/ast/ast_impl.h"
#include "common/expr.h"
#include "common/native_type.h"
#include "common/type_reflector.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/trace_step.h"
#include "internal/casts.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

// Class representing a CEL program being built.
//
// Maintains tree structure and mapping from the AST representation to
// subexpressions. Maintains an insertion point for new steps and
// subexpressions.
//
// This class is thread-hostile and not intended for direct access outside of
// the Expression builder. Extensions should interact with this through the
// the PlannerContext member functions.
class ProgramBuilder {
 public:
  class Subexpression;

 private:
  using SubprogramMap =
      absl::flat_hash_map<const cel::Expr*,
                          std::unique_ptr<ProgramBuilder::Subexpression>>;

 public:
  // Represents a subexpression.
  //
  // Steps apply operations on the stack machine for the C++ runtime.
  // For most expression types, this maps to a post order traversal -- for all
  // nodes, evaluate dependencies (pushing their results to stack) then evaluate
  // self.
  //
  // Must be tied to a ProgramBuilder to coordinate relationships.
  class Subexpression {
   private:
    using Element = absl::variant<std::unique_ptr<ExpressionStep>,
                                  Subexpression* ABSL_NONNULL>;

    using TreePlan = std::vector<Element>;
    using FlattenedPlan = std::vector<std::unique_ptr<const ExpressionStep>>;

   public:
    struct RecursiveProgram {
      std::unique_ptr<DirectExpressionStep> step;
      int depth;
    };

    ~Subexpression() = default;

    // Not copyable or movable.
    Subexpression(const Subexpression&) = delete;
    Subexpression& operator=(const Subexpression&) = delete;
    Subexpression(Subexpression&&) = delete;
    Subexpression& operator=(Subexpression&&) = delete;

    // Add a program step at the current end of the subexpression.
    bool AddStep(std::unique_ptr<ExpressionStep> step) {
      if (IsRecursive()) {
        return false;
      }

      if (IsFlattened()) {
        flattened_elements().push_back(std::move(step));
        return true;
      }

      elements().push_back({std::move(step)});
      return true;
    }

    void AddSubexpression(Subexpression* ABSL_NONNULL expr) {
      ABSL_DCHECK(absl::holds_alternative<TreePlan>(program_));
      ABSL_DCHECK(owner_ == expr->owner_);
      elements().push_back(expr);
    }

    // Accessor for elements (either simple steps or subexpressions).
    //
    // Value is undefined if in the expression has already been flattened.
    std::vector<Element>& elements() {
      ABSL_DCHECK(absl::holds_alternative<TreePlan>(program_));
      return absl::get<TreePlan>(program_);
    }

    const std::vector<Element>& elements() const {
      ABSL_DCHECK(absl::holds_alternative<TreePlan>(program_));
      return absl::get<TreePlan>(program_);
    }

    // Accessor for program steps.
    //
    // Value is undefined if in the expression has not yet been flattened.
    std::vector<std::unique_ptr<const ExpressionStep>>& flattened_elements() {
      ABSL_DCHECK(IsFlattened());
      return absl::get<FlattenedPlan>(program_);
    }

    const std::vector<std::unique_ptr<const ExpressionStep>>&
    flattened_elements() const {
      ABSL_DCHECK(IsFlattened());
      return absl::get<FlattenedPlan>(program_);
    }

    void set_recursive_program(std::unique_ptr<DirectExpressionStep> step,
                               int depth) {
      program_ = RecursiveProgram{std::move(step), depth};
    }

    const RecursiveProgram& recursive_program() const {
      ABSL_DCHECK(IsRecursive());
      return absl::get<RecursiveProgram>(program_);
    }

    absl::optional<int> RecursiveDependencyDepth() const;

    std::vector<std::unique_ptr<DirectExpressionStep>>
    ExtractRecursiveDependencies() const;

    RecursiveProgram ExtractRecursiveProgram();

    bool IsRecursive() const {
      return absl::holds_alternative<RecursiveProgram>(program_);
    }

    // Compute the current number of program steps in this subexpression and
    // its dependencies.
    size_t ComputeSize() const;

    // Calculate the number of steps from the end of base to before target,
    // (including negative offsets).
    int CalculateOffset(int base, int target) const;

    // Extract a child subexpression.
    //
    // The expression is removed from the elements array.
    //
    // Returns nullptr if child is not an element of this subexpression.
    Subexpression* ABSL_NULLABLE ExtractChild(Subexpression* child);

    // Flatten the subexpression.
    //
    // This removes the structure tracking for subexpressions, but makes the
    // subprogram evaluable on the runtime's stack machine.
    void Flatten();

    bool IsFlattened() const {
      return absl::holds_alternative<FlattenedPlan>(program_);
    }

    // Extract a flattened subexpression into the given vector. Transferring
    // ownership of the given steps.
    //
    // Returns false if the subexpression is not currently flattened.
    bool ExtractTo(std::vector<std::unique_ptr<const ExpressionStep>>& out);

   private:
    Subexpression(const cel::Expr* self, ProgramBuilder* owner);

    friend class ProgramBuilder;

    // Some extensions expect the program plan to be contiguous mid-planning.
    //
    // This adds complexity, but supports swapping to a flat representation as
    // needed.
    absl::variant<TreePlan, FlattenedPlan, RecursiveProgram> program_;

    const cel::Expr* self_;
    const cel::Expr* ABSL_NULLABLE parent_;
    ProgramBuilder* owner_;
  };

  ProgramBuilder();

  // Flatten the main subexpression and return its value.
  //
  // This transfers ownership of the program, returning the builder to starting
  // state. (See FlattenSubexpressions).
  ExecutionPath FlattenMain();

  // Flatten extracted subprograms.
  //
  // This transfers ownership of the subprograms, returning the extracted
  // programs table to starting state.
  std::vector<ExecutionPath> FlattenSubexpressions();

  // Returns the current subexpression where steps and new subexpressions are
  // added.
  //
  // May return null if the builder is not currently planning an expression.
  Subexpression* ABSL_NULLABLE current() { return current_; }

  // Enter a subexpression context.
  //
  // Adds a subexpression at the current insertion point and move insertion
  // to the subexpression.
  //
  // Returns the new current() value.
  //
  // May return nullptr if the expression is already indexed in the program
  // builder.
  Subexpression* ABSL_NULLABLE EnterSubexpression(const cel::Expr* expr,
                                                  size_t size_hint = 0);

  // Exit a subexpression context.
  //
  // Sets insertion point to parent.
  //
  // Returns the new current() value or nullptr if called out of order.
  Subexpression* ABSL_NULLABLE ExitSubexpression(const cel::Expr* expr);

  // Return the subexpression mapped to the given expression.
  //
  // Returns nullptr if the mapping doesn't exist either due to the
  // program being overwritten or not encountering the expression.
  Subexpression* ABSL_NULLABLE GetSubexpression(const cel::Expr* expr);

  // Return the extracted subexpression mapped to the given index.
  //
  // Returns nullptr if the mapping doesn't exist
  Subexpression* ABSL_NULLABLE GetExtractedSubexpression(size_t index) {
    if (index >= extracted_subexpressions_.size()) {
      return nullptr;
    }

    return extracted_subexpressions_[index];
  }

  // Return index to the extracted subexpression.
  //
  // Returns -1 if the subexpression is not found.
  int ExtractSubexpression(const cel::Expr* expr);

  // Add a program step to the current subexpression.
  // If successful, returns the step pointer.
  //
  // Note: If successful, the pointer should remain valid until the parent
  // expression is finalized. Optimizers may modify the program plan which may
  // free the step at that point.
  ExpressionStep* ABSL_NULLABLE AddStep(std::unique_ptr<ExpressionStep> step);

  void Reset();

 private:
  static std::vector<std::unique_ptr<const ExpressionStep>>
  FlattenSubexpression(Subexpression* ABSL_NONNULL expr);

  Subexpression* ABSL_NULLABLE MakeSubexpression(const cel::Expr* expr);

  Subexpression* ABSL_NULLABLE root_;
  std::vector<Subexpression* ABSL_NONNULL> extracted_subexpressions_;
  Subexpression* ABSL_NULLABLE current_;
  SubprogramMap subprogram_map_;
};

// Attempt to downcast a specific type of recursive step.
template <typename Subclass>
const Subclass* TryDowncastDirectStep(const DirectExpressionStep* step) {
  if (step == nullptr) {
    return nullptr;
  }

  auto type_id = step->GetNativeTypeId();
  if (type_id == cel::NativeTypeId::For<TraceStep>()) {
    const auto* trace_step = cel::internal::down_cast<const TraceStep*>(step);
    auto deps = trace_step->GetDependencies();
    if (!deps.has_value() || deps->size() != 1) {
      return nullptr;
    }
    step = deps->at(0);
    type_id = step->GetNativeTypeId();
  }

  if (type_id == cel::NativeTypeId::For<Subclass>()) {
    return cel::internal::down_cast<const Subclass*>(step);
  }

  return nullptr;
}

// Class representing FlatExpr internals exposed to extensions.
class PlannerContext {
 public:
  PlannerContext(
      std::shared_ptr<const cel::runtime_internal::RuntimeEnv> environment,
      const Resolver& resolver, const cel::RuntimeOptions& options,
      const cel::TypeReflector& type_reflector,
      cel::runtime_internal::IssueCollector& issue_collector,
      ProgramBuilder& program_builder,
      std::shared_ptr<google::protobuf::Arena>& arena ABSL_ATTRIBUTE_LIFETIME_BOUND,
      std::shared_ptr<google::protobuf::MessageFactory> message_factory = nullptr)
      : environment_(std::move(environment)),
        resolver_(resolver),
        type_reflector_(type_reflector),
        options_(options),
        issue_collector_(issue_collector),
        program_builder_(program_builder),
        arena_(arena),
        explicit_arena_(arena_ != nullptr),
        message_factory_(std::move(message_factory)) {}

  ProgramBuilder& program_builder() { return program_builder_; }

  // Returns true if the subplan is inspectable.
  //
  // If false, the node is not mapped to a subexpression in the program builder.
  bool IsSubplanInspectable(const cel::Expr& node) const;

  // Return a view to the current subplan representing node.
  //
  // Note: this is invalidated after a sibling or parent is updated.
  //
  // This operation forces the subexpression to flatten which removes the
  // expr->program mapping for any descendants.
  ExecutionPathView GetSubplan(const cel::Expr& node);

  // Extract the plan steps for the given expr.
  //
  // After successful extraction, the subexpression is still inspectable, but
  // empty.
  absl::StatusOr<ExecutionPath> ExtractSubplan(const cel::Expr& node);

  // Replace the subplan associated with node with a new subplan.
  //
  // This operation forces the subexpression to flatten which removes the
  // expr->program mapping for any descendants.
  absl::Status ReplaceSubplan(const cel::Expr& node, ExecutionPath path);

  // Replace the subplan associated with node with a new recursive subplan.
  //
  // This operation clears any existing plan to which removes the
  // expr->program mapping for any descendants.
  absl::Status ReplaceSubplan(const cel::Expr& node,
                              std::unique_ptr<DirectExpressionStep> step,
                              int depth);

  // Extend the current subplan with the given expression step.
  absl::Status AddSubplanStep(const cel::Expr& node,
                              std::unique_ptr<ExpressionStep> step);

  const Resolver& resolver() const { return resolver_; }
  const cel::TypeReflector& type_reflector() const { return type_reflector_; }
  const cel::RuntimeOptions& options() const { return options_; }
  cel::runtime_internal::IssueCollector& issue_collector() {
    return issue_collector_;
  }

  const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool() const {
    return environment_->descriptor_pool.get();
  }

  // Returns `true` if an arena was explicitly provided during planning.
  bool HasExplicitArena() const { return explicit_arena_; }

  google::protobuf::Arena* ABSL_NONNULL MutableArena() {
    if (!explicit_arena_ && arena_ == nullptr) {
      arena_ = std::make_shared<google::protobuf::Arena>();
    }
    ABSL_DCHECK(arena_ != nullptr);
    return arena_.get();
  }

  // Returns `true` if a message factory was explicitly provided during
  // planning.
  bool HasExplicitMessageFactory() const { return message_factory_ != nullptr; }

  google::protobuf::MessageFactory* ABSL_NONNULL MutableMessageFactory() {
    return HasExplicitMessageFactory() ? message_factory_.get()
                                       : environment_->MutableMessageFactory();
  }

 private:
  const std::shared_ptr<const cel::runtime_internal::RuntimeEnv> environment_;
  const Resolver& resolver_;
  const cel::TypeReflector& type_reflector_;
  const cel::RuntimeOptions& options_;
  cel::runtime_internal::IssueCollector& issue_collector_;
  ProgramBuilder& program_builder_;
  std::shared_ptr<google::protobuf::Arena>& arena_;
  const bool explicit_arena_;
  const std::shared_ptr<google::protobuf::MessageFactory> message_factory_;
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
                                  const cel::Expr& node) = 0;

  // Called after planning the given expr node.
  virtual absl::Status OnPostVisit(PlannerContext& context,
                                   const cel::Expr& node) = 0;
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
