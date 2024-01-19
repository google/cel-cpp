/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval/compiler/flat_expr_builder.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/ast.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/builtins.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_provider.h"
#include "base/value_manager.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/comprehension_step.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/container_access_step.h"
#include "eval/eval/create_list_step.h"
#include "eval/eval/create_struct_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/function_step.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/jump_step.h"
#include "eval/eval/lazy_init_step.h"
#include "eval/eval/logic_step.h"
#include "eval/eval/select_step.h"
#include "eval/eval/shadowable_value_step.h"
#include "eval/eval/ternary_step.h"
#include "eval/public/ast_traverse_native.h"
#include "eval/public/ast_visitor_native.h"
#include "eval/public/source_position_native.h"
#include "internal/status_macros.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/runtime_issue.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Ast;
using ::cel::Handle;
using ::cel::RuntimeIssue;
using ::cel::TypeFactory;
using ::cel::TypeManager;
using ::cel::Value;
using ::cel::ValueManager;
using ::cel::ast_internal::AstImpl;
using ::cel::ast_internal::AstTraverse;
using ::cel::runtime_internal::IssueCollector;

// Forward declare to resolve circular dependency for short_circuiting visitors.
class FlatExprVisitor;

// Representation of extracted subexpressions.
using ExpressionTable = std::vector<ExecutionPath>;

// Helper for bookkeeping variables mapped to indexes.
class IndexManager {
 public:
  IndexManager() : next_free_slot_(0), max_slot_count_(0) {}

  size_t ReserveSlots(size_t n) {
    size_t result = next_free_slot_;
    next_free_slot_ += n;
    if (next_free_slot_ > max_slot_count_) {
      max_slot_count_ = next_free_slot_;
    }
    return result;
  }

  size_t ReleaseSlots(size_t n) {
    next_free_slot_ -= n;
    return next_free_slot_;
  }

  size_t max_slot_count() const { return max_slot_count_; }

 private:
  size_t next_free_slot_;
  size_t max_slot_count_;
};

// A convenience wrapper for offset-calculating logic.
class Jump {
 public:
  explicit Jump() : self_index_(-1), jump_step_(nullptr) {}
  explicit Jump(int self_index, JumpStepBase* jump_step)
      : self_index_(self_index), jump_step_(jump_step) {}
  void set_target(int index) {
    // 0 offset means no-op.
    jump_step_->set_jump_offset(index - self_index_ - 1);
  }
  bool exists() { return jump_step_ != nullptr; }

 private:
  int self_index_;
  JumpStepBase* jump_step_;
};

class CondVisitor {
 public:
  virtual ~CondVisitor() = default;
  virtual void PreVisit(const cel::ast_internal::Expr* expr) = 0;
  virtual void PostVisitArg(int arg_num,
                            const cel::ast_internal::Expr* expr) = 0;
  virtual void PostVisit(const cel::ast_internal::Expr* expr) = 0;
};

// Visitor managing the "&&" and "||" operatiions.
// Implements short-circuiting if enabled.
//
// With short-circuiting enabled, generates a program like:
//   +-------------+------------------------+-----------------------+
//   | PC          | Step                   | Stack                 |
//   +-------------+------------------------+-----------------------+
//   | i + 0       | <Arg1>                 | arg1                  |
//   | i + 1       | ConditionalJump i + 4  | arg1                  |
//   | i + 2       | <Arg2>                 | arg1, arg2            |
//   | i + 3       | BooleanOperator        | Op(arg1, arg2)        |
//   | i + 4       | <rest of program>      | arg1 | Op(arg1, arg2) |
//   +-------------+------------------------+------------------------+
class BinaryCondVisitor : public CondVisitor {
 public:
  explicit BinaryCondVisitor(FlatExprVisitor* visitor, bool cond_value,
                             bool short_circuiting)
      : visitor_(visitor),
        cond_value_(cond_value),
        short_circuiting_(short_circuiting) {}

  void PreVisit(const cel::ast_internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast_internal::Expr* expr) override;
  void PostVisit(const cel::ast_internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  const bool cond_value_;
  Jump jump_step_;
  bool short_circuiting_;
};

class TernaryCondVisitor : public CondVisitor {
 public:
  explicit TernaryCondVisitor(FlatExprVisitor* visitor) : visitor_(visitor) {}

  void PreVisit(const cel::ast_internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast_internal::Expr* expr) override;
  void PostVisit(const cel::ast_internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  Jump jump_to_second_;
  Jump error_jump_;
  Jump jump_after_first_;
};

class ExhaustiveTernaryCondVisitor : public CondVisitor {
 public:
  explicit ExhaustiveTernaryCondVisitor(FlatExprVisitor* visitor)
      : visitor_(visitor) {}

  void PreVisit(const cel::ast_internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast_internal::Expr* expr) override {
  }
  void PostVisit(const cel::ast_internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
};

// Returns whether this comprehension appears to be a standard map/filter
// macro implementation. It is not exhaustive, so it is unsafe to use with
// custom comprehensions outside of the standard macros or hand crafted ASTs.
bool IsOptimizableListAppend(
    const cel::ast_internal::Comprehension* comprehension,
    bool enable_comprehension_list_append) {
  if (!enable_comprehension_list_append) {
    return false;
  }
  absl::string_view accu_var = comprehension->accu_var();
  if (accu_var.empty() ||
      comprehension->result().ident_expr().name() != accu_var) {
    return false;
  }
  if (!comprehension->accu_init().has_list_expr()) {
    return false;
  }

  if (!comprehension->loop_step().has_call_expr()) {
    return false;
  }

  // Macro loop_step for a filter() will contain a ternary:
  //   filter ? accu_var + [elem] : accu_var
  // Macro loop_step for a map() will contain a list concat operation:
  //   accu_var + [elem]
  const auto* call_expr = &comprehension->loop_step().call_expr();

  if (call_expr->function() == cel::builtin::kTernary &&
      call_expr->args().size() == 3) {
    if (!call_expr->args()[1].has_call_expr()) {
      return false;
    }
    call_expr = &(call_expr->args()[1].call_expr());
  }

  return call_expr->function() == cel::builtin::kAdd &&
         call_expr->args().size() == 2 &&
         call_expr->args()[0].has_ident_expr() &&
         call_expr->args()[0].ident_expr().name() == accu_var;
}

bool IsBind(const cel::ast_internal::Comprehension* comprehension) {
  static constexpr absl::string_view kUnusedIterVar = "#unused";

  return comprehension->loop_condition().const_expr().has_bool_value() &&
         comprehension->loop_condition().const_expr().bool_value() == false &&
         comprehension->iter_var() == kUnusedIterVar &&
         comprehension->iter_range().has_list_expr() &&
         comprehension->iter_range().list_expr().elements().empty();
}

// Visitor for Comprehension expressions.
class ComprehensionVisitor {
 public:
  explicit ComprehensionVisitor(FlatExprVisitor* visitor, bool short_circuiting,
                                bool is_trivial, size_t iter_slot,
                                size_t accu_slot)
      : visitor_(visitor),
        next_step_(nullptr),
        cond_step_(nullptr),
        short_circuiting_(short_circuiting),
        is_trivial_(is_trivial),
        accu_init_extracted_(false),
        iter_slot_(iter_slot),
        accu_slot_(accu_slot) {}

  void PreVisit(const cel::ast_internal::Expr* expr);
  void PostVisitArg(cel::ast_internal::ComprehensionArg arg_num,
                    const cel::ast_internal::Expr* comprehension_expr) {
    if (is_trivial_) {
      PostVisitArgTrivial(arg_num, comprehension_expr);
    } else {
      PostVisitArgDefault(arg_num, comprehension_expr);
    }
  }
  void PostVisit(const cel::ast_internal::Expr* expr);

  void MarkAccuInitExtracted() { accu_init_extracted_ = true; }

 private:
  void PostVisitArgTrivial(cel::ast_internal::ComprehensionArg arg_num,
                           const cel::ast_internal::Expr* comprehension_expr);

  void PostVisitArgDefault(cel::ast_internal::ComprehensionArg arg_num,
                           const cel::ast_internal::Expr* comprehension_expr);

  FlatExprVisitor* visitor_;
  ComprehensionNextStep* next_step_;
  ComprehensionCondStep* cond_step_;
  int next_step_pos_;
  int cond_step_pos_;
  bool short_circuiting_;
  bool is_trivial_;
  bool accu_init_extracted_;
  size_t iter_slot_;
  size_t accu_slot_;
};

class FlatExprVisitor : public cel::ast_internal::AstVisitor {
 public:
  FlatExprVisitor(
      const Resolver& resolver, const cel::RuntimeOptions& options,
      std::vector<std::unique_ptr<ProgramOptimizer>> program_optimizers,
      const absl::flat_hash_map<int64_t, cel::ast_internal::Reference>&
          reference_map,
      ExpressionTable& expression_table, ExecutionPath& path,
      ValueManager& value_factory, IssueCollector& issue_collector,
      PlannerContext::ProgramTree& program_tree,
      PlannerContext& extension_context)
      : resolver_(resolver),
        expression_table_(expression_table),
        execution_path_(path),
        value_factory_(value_factory),
        progress_status_(absl::OkStatus()),
        resolved_select_expr_(nullptr),
        parent_expr_(nullptr),
        options_(options),
        program_optimizers_(std::move(program_optimizers)),
        issue_collector_(issue_collector),
        program_tree_(program_tree),
        extension_context_(extension_context) {}

  void PreVisitExpr(const cel::ast_internal::Expr* expr,
                    const cel::ast_internal::SourcePosition*) override {
    ValidateOrError(
        !absl::holds_alternative<absl::monostate>(expr->expr_kind()),
        "Invalid empty expression");
    if (!progress_status_.ok()) {
      return;
    }
    if (resume_from_suppressed_branch_ == nullptr &&
        suppressed_branches_.find(expr) != suppressed_branches_.end()) {
      resume_from_suppressed_branch_ = expr;
    }
    if (!ProgramStructureTrackingEnabled()) {
      return;
    }
    PlannerContext::ProgramInfo& info = program_tree_[expr];
    info.range_start = GetCurrentIndex();
    info.parent = parent_expr_;
    if (parent_expr_ != nullptr) {
      program_tree_[parent_expr_].children.push_back(expr);
    }
    parent_expr_ = expr;

    for (const std::unique_ptr<ProgramOptimizer>& optimizer :
         program_optimizers_) {
      absl::Status status = optimizer->OnPreVisit(extension_context_, *expr);
      if (!status.ok()) {
        SetProgressStatusError(status);
      }
    }
  }

  void PostVisitExpr(const cel::ast_internal::Expr* expr,
                     const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (expr == resume_from_suppressed_branch_) {
      resume_from_suppressed_branch_ = nullptr;
    }
    if (!ProgramStructureTrackingEnabled()) {
      return;
    }
    PlannerContext::ProgramInfo& info = program_tree_[expr];
    info.range_len = GetCurrentIndex() - info.range_start;
    parent_expr_ = info.parent;

    for (const std::unique_ptr<ProgramOptimizer>& optimizer :
         program_optimizers_) {
      absl::Status status = optimizer->OnPostVisit(extension_context_, *expr);
      if (!status.ok()) {
        SetProgressStatusError(status);
        return;
      }
    }
    if (options_.enable_lazy_bind_initialization &&
        !comprehension_stack_.empty() &&
        (&comprehension_stack_.back().comprehension->accu_init() == expr)) {
      SetProgressStatusError(
          MaybeExtractSubexpression(expr, comprehension_stack_.back()));
    }
  }

  void PostVisitConst(const cel::ast_internal::Constant* const_expr,
                      const cel::ast_internal::Expr* expr,
                      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    AddStep(CreateConstValueStep(*const_expr, expr->id(), value_factory_));
  }

  struct SlotLookupResult {
    int slot;
    int subexpression;
  };

  // Helper to lookup a variable mapped to a slot.
  //
  // If lazy evaluation enabled and ided as a lazy expression,
  // subexpression and slot will be set.
  SlotLookupResult LookupSlot(absl::string_view path) {
    if (!comprehension_stack_.empty()) {
      for (int i = comprehension_stack_.size() - 1; i >= 0; i--) {
        const ComprehensionStackRecord& record = comprehension_stack_[i];
        if (record.iter_var_in_scope &&
            record.comprehension->iter_var() == path) {
          if (record.is_optimizable_bind) {
            SetProgressStatusError(issue_collector_.AddIssue(
                RuntimeIssue::CreateWarning(absl::InvalidArgumentError(
                    "Unexpected iter_var access in trivial comprehension"))));
            return {-1, -1};
          }
          return {static_cast<int>(record.iter_slot), -1};
        }
        if (record.accu_var_in_scope &&
            record.comprehension->accu_var() == path) {
          int slot = record.accu_slot;
          int subexpression = -1;
          if (record.should_lazy_eval) {
            subexpression = record.subexpression;
          }
          return {slot, subexpression};
        }
      }
    }
    return {-1, -1};
  }

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const cel::ast_internal::Ident* ident_expr,
                      const cel::ast_internal::Expr* expr,
                      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    const std::string& path = ident_expr->name();
    if (!ValidateOrError(
            !path.empty(),
            "Invalid expression: identifier 'name' must not be empty")) {
      return;
    }

    // Attempt to resolve a select expression as a namespaced identifier for an
    // enum or type constant value.
    while (!namespace_stack_.empty()) {
      const auto& select_node = namespace_stack_.front();
      // Generate path in format "<ident>.<field 0>.<field 1>...".
      auto select_expr = select_node.first;
      auto qualified_path = absl::StrCat(path, ".", select_node.second);
      namespace_map_[select_expr] = qualified_path;

      // Attempt to find a constant enum or type value which matches the
      // qualified path present in the expression. Whether the identifier
      // can be resolved to a type instance depends on whether the option to
      // 'enable_qualified_type_identifiers' is set to true.
      Handle<Value> const_value =
          resolver_.FindConstant(qualified_path, select_expr->id());
      if (const_value) {
        AddStep(CreateShadowableValueStep(
            qualified_path, std::move(const_value), select_expr->id()));
        resolved_select_expr_ = select_expr;
        namespace_stack_.clear();
        return;
      }
      namespace_stack_.pop_front();
    }

    // Attempt to resolve a simple identifier as an enum or type constant value.
    Handle<Value> const_value = resolver_.FindConstant(path, expr->id());
    if (const_value) {
      AddStep(
          CreateShadowableValueStep(path, std::move(const_value), expr->id()));
      return;
    }

    // If this is a comprehension variable, check for the assigned slot.
    SlotLookupResult slot = LookupSlot(path);

    if (slot.subexpression >= 0) {
      AddStep(
          CreateCheckLazyInitStep(slot.slot, slot.subexpression, expr->id()));
      AddStep(CreateAssignSlotStep(slot.slot));
      return;
    } else if (slot.slot >= 0) {
      AddStep(CreateIdentStepForSlot(*ident_expr, slot.slot, expr->id()));
      return;
    }

    AddStep(CreateIdentStep(*ident_expr, expr->id()));
  }

  void PreVisitSelect(const cel::ast_internal::Select* select_expr,
                      const cel::ast_internal::Expr* expr,
                      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!ValidateOrError(
            !select_expr->field().empty(),
            "Invalid expression: select 'field' must not be empty")) {
      return;
    }

    // Not exactly the cleanest solution - we peek into child of
    // select_expr.
    // Chain of multiple SELECT ending with IDENT can represent namespaced
    // entity.
    if (!select_expr->test_only() &&
        (select_expr->operand().has_ident_expr() ||
         select_expr->operand().has_select_expr())) {
      // select expressions are pushed in reverse order:
      // google.type.Expr is pushed as:
      // - field: 'Expr'
      // - field: 'type'
      // - id: 'google'
      //
      // The search order though is as follows:
      // - id: 'google.type.Expr'
      // - id: 'google.type', field: 'Expr'
      // - id: 'google', field: 'type', field: 'Expr'
      for (size_t i = 0; i < namespace_stack_.size(); i++) {
        auto ns = namespace_stack_[i];
        namespace_stack_[i] = {
            ns.first, absl::StrCat(select_expr->field(), ".", ns.second)};
      }
      namespace_stack_.push_back({expr, select_expr->field()});
    } else {
      namespace_stack_.clear();
    }
  }

  // Select node handler.
  // Invoked after child nodes are processed.
  void PostVisitSelect(const cel::ast_internal::Select* select_expr,
                       const cel::ast_internal::Expr* expr,
                       const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    // Check if we are "in the middle" of namespaced name.
    // This is currently enum specific. Constant expression that corresponds
    // to resolved enum value has been already created, thus preceding chain
    // of selects is no longer relevant.
    if (resolved_select_expr_) {
      if (expr == resolved_select_expr_) {
        resolved_select_expr_ = nullptr;
      }
      return;
    }

    std::string select_path = "";
    auto it = namespace_map_.find(expr);
    if (it != namespace_map_.end()) {
      select_path = it->second;
    }

    AddStep(CreateSelectStep(*select_expr, expr->id(), select_path,
                             options_.enable_empty_wrapper_null_unboxing,
                             value_factory_));
  }

  // Call node handler group.
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const cel::ast_internal::Call* call_expr,
                    const cel::ast_internal::Expr* expr,
                    const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    std::unique_ptr<CondVisitor> cond_visitor;
    if (call_expr->function() == cel::builtin::kAnd) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ false, options_.short_circuiting);
    } else if (call_expr->function() == cel::builtin::kOr) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ true, options_.short_circuiting);
    } else if (call_expr->function() == cel::builtin::kTernary) {
      if (options_.short_circuiting) {
        cond_visitor = std::make_unique<TernaryCondVisitor>(this);
      } else {
        cond_visitor = std::make_unique<ExhaustiveTernaryCondVisitor>(this);
      }
    } else {
      return;
    }

    if (cond_visitor) {
      cond_visitor->PreVisit(expr);
      cond_visitor_stack_.push({expr, std::move(cond_visitor)});
    }
  }

  // Invoked after all child nodes are processed.
  void PostVisitCall(const cel::ast_internal::Call* call_expr,
                     const cel::ast_internal::Expr* expr,
                     const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    auto cond_visitor = FindCondVisitor(expr);
    if (cond_visitor) {
      cond_visitor->PostVisit(expr);
      cond_visitor_stack_.pop();
      return;
    }

    // Special case for "_[_]".
    if (call_expr->function() == cel::builtin::kIndex) {
      AddStep(CreateContainerAccessStep(*call_expr, expr->id()));
      return;
    }

    // Establish the search criteria for a given function.
    absl::string_view function = call_expr->function();
    bool receiver_style = call_expr->has_target();
    size_t num_args = call_expr->args().size() + (receiver_style ? 1 : 0);
    auto arguments_matcher = ArgumentsMatcher(num_args);

    // Check to see if this is a special case of add that should really be
    // treated as a list append
    if (!comprehension_stack_.empty() &&
        comprehension_stack_.back().is_optimizable_list_append) {
      // Already checked that this is an optimizeable comprehension,
      // check that this is the correct list append node.
      const cel::ast_internal::Comprehension* comprehension =
          comprehension_stack_.back().comprehension;
      const cel::ast_internal::Expr& loop_step = comprehension->loop_step();
      // Macro loop_step for a map() will contain a list concat operation:
      //   accu_var + [elem]
      if (&loop_step == expr) {
        function = cel::builtin::kRuntimeListAppend;
      }
      // Macro loop_step for a filter() will contain a ternary:
      //   filter ? accu_var + [elem] : accu_var
      if (loop_step.has_call_expr() &&
          loop_step.call_expr().function() == cel::builtin::kTernary &&
          loop_step.call_expr().args().size() == 3 &&
          &(loop_step.call_expr().args()[1]) == expr) {
        function = cel::builtin::kRuntimeListAppend;
      }
    }

    // First, search for lazily defined function overloads.
    // Lazy functions shadow eager functions with the same signature.
    auto lazy_overloads = resolver_.FindLazyOverloads(
        function, receiver_style, arguments_matcher, expr->id());
    if (!lazy_overloads.empty()) {
      AddStep(CreateFunctionStep(*call_expr, expr->id(),
                                 std::move(lazy_overloads)));
      return;
    }

    // Second, search for eagerly defined function overloads.
    auto overloads = resolver_.FindOverloads(function, receiver_style,
                                             arguments_matcher, expr->id());
    if (overloads.empty()) {
      // Create a warning that the overload could not be found. Depending on the
      // builder_warnings configuration, this could result in termination of the
      // CelExpression creation or an inspectable warning for use within runtime
      // logging.
      auto status = issue_collector_.AddIssue(RuntimeIssue::CreateWarning(
          absl::InvalidArgumentError(
              "No overloads provided for FunctionStep creation"),
          RuntimeIssue::ErrorCode::kNoMatchingOverload));
      if (!status.ok()) {
        SetProgressStatusError(status);
        return;
      }
    }
    AddStep(CreateFunctionStep(*call_expr, expr->id(), std::move(overloads)));
  }

  void PreVisitComprehension(
      const cel::ast_internal::Comprehension* comprehension,
      const cel::ast_internal::Expr* expr,
      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!ValidateOrError(options_.enable_comprehension,
                         "Comprehension support is disabled")) {
      return;
    }
    const auto& accu_var = comprehension->accu_var();
    const auto& iter_var = comprehension->iter_var();
    ValidateOrError(!accu_var.empty(),
                    "Invalid comprehension: 'accu_var' must not be empty");
    ValidateOrError(!iter_var.empty(),
                    "Invalid comprehension: 'iter_var' must not be empty");
    ValidateOrError(
        accu_var != iter_var,
        "Invalid comprehension: 'accu_var' must not be the same as 'iter_var'");
    ValidateOrError(comprehension->has_accu_init(),
                    "Invalid comprehension: 'accu_init' must be set");
    ValidateOrError(comprehension->has_loop_condition(),
                    "Invalid comprehension: 'loop_condition' must be set");
    ValidateOrError(comprehension->has_loop_step(),
                    "Invalid comprehension: 'loop_step' must be set");
    ValidateOrError(comprehension->has_result(),
                    "Invalid comprehension: 'result' must be set");

    size_t iter_slot, accu_slot;
    bool is_bind = IsBind(comprehension);
    if (is_bind) {
      accu_slot = iter_slot = index_manager_.ReserveSlots(1);
    } else {
      iter_slot = index_manager_.ReserveSlots(2);
      accu_slot = iter_slot + 1;
    }
    comprehension_stack_.push_back(
        {expr, comprehension, iter_slot, accu_slot,
         /*subexpression=*/-1,
         IsOptimizableListAppend(comprehension,
                                 options_.enable_comprehension_list_append),
         is_bind,
         /*.iter_var_in_scope=*/false,
         /*.accu_var_in_scope=*/false,
         /*.in_accu_init=*/false,
         /*.should_lazy_eval=*/is_bind &&
             options_.enable_lazy_bind_initialization,
         std::make_unique<ComprehensionVisitor>(
             this, options_.short_circuiting, is_bind, iter_slot, accu_slot)});
    comprehension_stack_.back().visitor->PreVisit(expr);
  }

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(
      const cel::ast_internal::Comprehension* comprehension_expr,
      const cel::ast_internal::Expr* expr,
      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    ComprehensionStackRecord& record = comprehension_stack_.back();
    if (comprehension_stack_.empty() ||
        record.comprehension != comprehension_expr) {
      return;
    }

    record.visitor->PostVisit(expr);

    // TODO(uncreated-issue/64): lazy binds complicate slot assignments because we may
    // require a slot outside of the scope expected from the AST (e.g. both the
    // initialization and result expressions contain a comprehension).
    //
    // We should determine if compacting the slot assignments improves
    // performance enough to use a better heuristic, but for now use dedicated
    // slots inside all bind scopes.
    if (!(options_.enable_lazy_bind_initialization && InBindScope())) {
      // Otherwise release slots for reuse.
      //
      // Note: with binds, the released slot may be different than the
      // reserved one, but the prior assignment will no longer be in scope or
      // accessible.
      index_manager_.ReleaseSlots((record.is_optimizable_bind) ? 1 : 2);
    }
    comprehension_stack_.pop_back();
  }

  void PreVisitComprehensionSubexpression(
      const cel::ast_internal::Expr* subexpr,
      const cel::ast_internal::Comprehension* compr,
      cel::ast_internal::ComprehensionArg comprehension_arg,
      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    if (comprehension_stack_.empty() ||
        comprehension_stack_.back().comprehension != compr) {
      return;
    }

    ComprehensionStackRecord& record = comprehension_stack_.back();

    switch (comprehension_arg) {
      case cel::ast_internal::ITER_RANGE: {
        record.in_accu_init = false;
        record.iter_var_in_scope = false;
        record.accu_var_in_scope = false;
        break;
      }
      case cel::ast_internal::ACCU_INIT: {
        record.in_accu_init = true;
        record.iter_var_in_scope = false;
        record.accu_var_in_scope = false;
        break;
      }
      case cel::ast_internal::LOOP_CONDITION: {
        record.in_accu_init = false;
        record.iter_var_in_scope = true;
        record.accu_var_in_scope = true;
        break;
      }
      case cel::ast_internal::LOOP_STEP: {
        record.in_accu_init = false;
        record.iter_var_in_scope = true;
        record.accu_var_in_scope = true;
        break;
      }
      case cel::ast_internal::RESULT: {
        record.in_accu_init = false;
        record.iter_var_in_scope = false;
        record.accu_var_in_scope = true;
        break;
      }
    }
  }

  void PostVisitComprehensionSubexpression(
      const cel::ast_internal::Expr* subexpr,
      const cel::ast_internal::Comprehension* compr,
      cel::ast_internal::ComprehensionArg comprehension_arg,
      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    if (comprehension_stack_.empty() ||
        comprehension_stack_.back().comprehension != compr) {
      return;
    }

    comprehension_stack_.back().visitor->PostVisitArg(
        comprehension_arg, comprehension_stack_.back().expr);
  }

  // Invoked after each argument node processed.
  void PostVisitArg(int arg_num, const cel::ast_internal::Expr* expr,
                    const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    auto cond_visitor = FindCondVisitor(expr);
    if (cond_visitor) {
      cond_visitor->PostVisitArg(arg_num, expr);
    }
  }

  // Nothing to do.
  void PostVisitTarget(const cel::ast_internal::Expr* expr,
                       const cel::ast_internal::SourcePosition*) override {}

  // CreateList node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateList(const cel::ast_internal::CreateList* list_expr,
                           const cel::ast_internal::Expr* expr,
                           const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!comprehension_stack_.empty()) {
      const ComprehensionStackRecord& comprehension =
          comprehension_stack_.back();
      if (comprehension.is_optimizable_list_append &&
          &(comprehension.comprehension->accu_init()) == expr) {
        AddStep(CreateCreateMutableListStep(*list_expr, expr->id()));
        return;
      }
    }
    AddStep(CreateCreateListStep(*list_expr, expr->id()));
  }

  // CreateStruct node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateStruct(
      const cel::ast_internal::CreateStruct* struct_expr,
      const cel::ast_internal::Expr* expr,
      const cel::ast_internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    // If the message name is empty, this signals that a map should be created.
    auto message_name = struct_expr->message_name();
    if (message_name.empty()) {
      for (const auto& entry : struct_expr->entries()) {
        ValidateOrError(entry.has_map_key(), "Map entry missing key");
        ValidateOrError(entry.has_value(), "Map entry missing value");
      }
      AddStep(CreateCreateStructStepForMap(*struct_expr, expr->id()));
      return;
    }

    // If the message name is not empty, then the message name must be resolved
    // within the container, and if a descriptor is found, then a proto message
    // creation step will be created.
    auto status_or_maybe_type = resolver_.FindType(message_name, expr->id());
    if (!status_or_maybe_type.ok()) {
      SetProgressStatusError(status_or_maybe_type.status());
      return;
    }
    if (ValidateOrError(status_or_maybe_type->has_value(),
                        "Invalid struct creation: missing type info for '",
                        message_name, "'")) {
      AddStep(CreateCreateStructStepForStruct(
          *struct_expr, (*status_or_maybe_type)->first,
          std::move((*status_or_maybe_type)->second), expr->id(),
          value_factory().type_manager()));
    }
  }

  absl::Status progress_status() const { return progress_status_; }

  cel::ValueManager& value_factory() { return value_factory_; }

  // Mark a branch as suppressed. The visitor will continue as normal, but
  // any emitted program steps are ignored.
  //
  // Only applies to branches that have not yet been visited (pre-order).
  void SuppressBranch(const cel::ast_internal::Expr* expr) {
    suppressed_branches_.insert(expr);
  }

  void AddStep(absl::StatusOr<std::unique_ptr<ExpressionStep>> step) {
    if (step.ok()) {
      AddStep(*std::move(step));
    } else {
      SetProgressStatusError(step.status());
    }
  }

  void AddStep(std::unique_ptr<ExpressionStep> step) {
    if (progress_status_.ok() && !PlanningSuppressed()) {
      execution_path_.push_back(std::move(step));
    }
  }

  void SetProgressStatusError(const absl::Status& status) {
    if (progress_status_.ok() && !status.ok()) {
      progress_status_ = status;
    }
  }

  // Index of the next step to be inserted.
  int GetCurrentIndex() const { return execution_path_.size(); }

  CondVisitor* FindCondVisitor(const cel::ast_internal::Expr* expr) const {
    if (cond_visitor_stack_.empty()) {
      return nullptr;
    }

    const auto& latest = cond_visitor_stack_.top();

    return (latest.first == expr) ? latest.second.get() : nullptr;
  }

  IndexManager& index_manager() { return index_manager_; }

  size_t slot_count() const { return index_manager_.max_slot_count(); }

  void AddOptimizer(std::unique_ptr<ProgramOptimizer> optimizer) {
    program_optimizers_.push_back(std::move(optimizer));
  }

  // Tests the boolean predicate, and if false produces an InvalidArgumentError
  // which concatenates the error_message and any optional message_parts as the
  // error status message.
  template <typename... MP>
  bool ValidateOrError(bool valid_expression, absl::string_view error_message,
                       MP... message_parts) {
    if (valid_expression) {
      return true;
    }
    SetProgressStatusError(absl::InvalidArgumentError(
        absl::StrCat(error_message, message_parts...)));
    return false;
  }

 private:
  struct ComprehensionStackRecord {
    const cel::ast_internal::Expr* expr;
    const cel::ast_internal::Comprehension* comprehension;
    size_t iter_slot;
    size_t accu_slot;
    // -1 indicates this shouldn't be used.
    int subexpression;
    bool is_optimizable_list_append;
    bool is_optimizable_bind;
    bool iter_var_in_scope;
    bool accu_var_in_scope;
    bool in_accu_init;
    bool should_lazy_eval;
    std::unique_ptr<ComprehensionVisitor> visitor;
  };

  bool PlanningSuppressed() const {
    return resume_from_suppressed_branch_ != nullptr;
  }

  bool ProgramStructureTrackingEnabled() {
    return options_.enable_lazy_bind_initialization ||
           !program_optimizers_.empty();
  }

  bool InBindScope() {
    for (const auto& record : comprehension_stack_) {
      if (record.is_optimizable_bind) {
        return true;
      }
    }
    return false;
  }

  absl::Status MaybeExtractSubexpression(const cel::ast_internal::Expr* expr,
                                         ComprehensionStackRecord& record) {
    if (!record.should_lazy_eval) {
      return absl::OkStatus();
    }
    CEL_ASSIGN_OR_RETURN(auto subexpr,
                         extension_context_.ExtractSubplan(*expr));

    CEL_RETURN_IF_ERROR(extension_context_.ReplaceSubplan(*expr, {}));

    expression_table_.push_back(std::move(subexpr));
    // off by one since mainline expression is handled separately.
    size_t id = expression_table_.size();
    record.subexpression = id;

    record.visitor->MarkAccuInitExtracted();

    return absl::OkStatus();
  }

  const Resolver& resolver_;
  ExpressionTable& expression_table_;
  ExecutionPath& execution_path_;
  ValueManager& value_factory_;
  absl::Status progress_status_;

  std::stack<
      std::pair<const cel::ast_internal::Expr*, std::unique_ptr<CondVisitor>>>
      cond_visitor_stack_;

  // Maps effective namespace names to Expr objects (IDENTs/SELECTs) that
  // define scopes for those namespaces.
  std::unordered_map<const cel::ast_internal::Expr*, std::string>
      namespace_map_;
  // Tracks SELECT-...SELECT-IDENT chains.
  std::deque<std::pair<const cel::ast_internal::Expr*, std::string>>
      namespace_stack_;

  // When multiple SELECT-...SELECT-IDENT chain is resolved as namespace, this
  // field is used as marker suppressing CelExpression creation for SELECTs.
  const cel::ast_internal::Expr* resolved_select_expr_;

  // Used for assembling a temporary tree mapping program segments
  // to source expr nodes.
  const cel::ast_internal::Expr* parent_expr_;

  const cel::RuntimeOptions& options_;

  std::vector<ComprehensionStackRecord> comprehension_stack_;
  absl::flat_hash_set<const cel::ast_internal::Expr*> suppressed_branches_;
  const cel::ast_internal::Expr* resume_from_suppressed_branch_ = nullptr;
  std::vector<std::unique_ptr<ProgramOptimizer>> program_optimizers_;
  IssueCollector& issue_collector_;

  PlannerContext::ProgramTree& program_tree_;
  PlannerContext extension_context_;
  IndexManager index_manager_;
};

void BinaryCondVisitor::PreVisit(const cel::ast_internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 2,
      "Invalid argument count for a binary function call.");
}

void BinaryCondVisitor::PostVisitArg(int arg_num,
                                     const cel::ast_internal::Expr* expr) {
  if (short_circuiting_ && arg_num == 0) {
    // If first branch evaluation result is enough to determine output,
    // jump over the second branch and provide result of the first argument as
    // final output.
    // Retain a pointer to the jump step so we can update the target after
    // planning the second argument.
    auto jump_step = CreateCondJumpStep(cond_value_, true, {}, expr->id());
    if (jump_step.ok()) {
      jump_step_ = Jump(visitor_->GetCurrentIndex(), jump_step->get());
    }
    visitor_->AddStep(std::move(jump_step));
  }
}

void BinaryCondVisitor::PostVisit(const cel::ast_internal::Expr* expr) {
  visitor_->AddStep((cond_value_) ? CreateOrStep(expr->id())
                                  : CreateAndStep(expr->id()));
  if (short_circuiting_) {
    // If shortcircuiting is enabled, point the conditional jump past the
    // boolean operator step.
    jump_step_.set_target(visitor_->GetCurrentIndex());
  }
}

void TernaryCondVisitor::PreVisit(const cel::ast_internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 3,
      "Invalid argument count for a ternary function call.");
}

void TernaryCondVisitor::PostVisitArg(int arg_num,
                                      const cel::ast_internal::Expr* expr) {
  // Ternary operator "_?_:_" requires a special handing.
  // In contrary to regular function call, its execution affects the control
  // flow of the overall CEL expression.
  // If condition value (argument 0) is True, then control flow is unaffected
  // as it is passed to the first conditional branch. Then, at the end of this
  // branch, the jump is performed over the second conditional branch.
  // If condition value is False, then jump is performed and control is passed
  // to the beginning of the second conditional branch.
  // If condition value is Error, then jump is peformed to bypass both
  // conditional branches and provide Error as result of ternary operation.

  // condition argument for ternary operator
  if (arg_num == 0) {
    // Jump in case of error or non-bool
    auto error_jump = CreateBoolCheckJumpStep({}, expr->id());
    if (error_jump.ok()) {
      error_jump_ = Jump(visitor_->GetCurrentIndex(), error_jump->get());
    }
    visitor_->AddStep(std::move(error_jump));

    // Jump to the second branch of execution
    // Value is to be removed from the stack.
    auto jump_to_second = CreateCondJumpStep(false, false, {}, expr->id());
    if (jump_to_second.ok()) {
      jump_to_second_ =
          Jump(visitor_->GetCurrentIndex(), jump_to_second->get());
    }
    visitor_->AddStep(std::move(jump_to_second));
  } else if (arg_num == 1) {
    // Jump after the first and over the second branch of execution.
    // Value is to be removed from the stack.
    auto jump_after_first = CreateJumpStep({}, expr->id());
    if (jump_after_first.ok()) {
      jump_after_first_ =
          Jump(visitor_->GetCurrentIndex(), jump_after_first->get());
    }
    visitor_->AddStep(std::move(jump_after_first));

    if (visitor_->ValidateOrError(
            jump_to_second_.exists(),
            "Error configuring ternary operator: jump_to_second_ is null")) {
      jump_to_second_.set_target(visitor_->GetCurrentIndex());
    }
  }
  // Code executed after traversing the final branch of execution
  // (arg_num == 2) is placed in PostVisitCall, to make this method less
  // clattered.
}

void TernaryCondVisitor::PostVisit(const cel::ast_internal::Expr*) {
  // Determine and set jump offset in jump instruction.
  if (visitor_->ValidateOrError(
          error_jump_.exists(),
          "Error configuring ternary operator: error_jump_ is null")) {
    error_jump_.set_target(visitor_->GetCurrentIndex());
  }
  if (visitor_->ValidateOrError(
          jump_after_first_.exists(),
          "Error configuring ternary operator: jump_after_first_ is null")) {
    jump_after_first_.set_target(visitor_->GetCurrentIndex());
  }
}

void ExhaustiveTernaryCondVisitor::PreVisit(
    const cel::ast_internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 3,
      "Invalid argument count for a ternary function call.");
}

void ExhaustiveTernaryCondVisitor::PostVisit(
    const cel::ast_internal::Expr* expr) {
  visitor_->AddStep(CreateTernaryStep(expr->id()));
}

void ComprehensionVisitor::PreVisit(const cel::ast_internal::Expr* expr) {
  if (is_trivial_) {
    visitor_->SuppressBranch(&expr->comprehension_expr().iter_range());
    visitor_->SuppressBranch(&expr->comprehension_expr().loop_condition());
    visitor_->SuppressBranch(&expr->comprehension_expr().loop_step());
  }
}

void ComprehensionVisitor::PostVisitArgDefault(
    cel::ast_internal::ComprehensionArg arg_num,
    const cel::ast_internal::Expr* expr) {
  switch (arg_num) {
    case cel::ast_internal::ITER_RANGE: {
      // post process iter_range to list its keys if it's a map
      // and initialize the loop index.
      visitor_->AddStep(CreateComprehensionInitStep(expr->id()));
      break;
    }
    case cel::ast_internal::ACCU_INIT: {
      next_step_pos_ = visitor_->GetCurrentIndex();
      next_step_ =
          new ComprehensionNextStep(iter_slot_, accu_slot_, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(next_step_));
      break;
    }
    case cel::ast_internal::LOOP_CONDITION: {
      cond_step_pos_ = visitor_->GetCurrentIndex();
      cond_step_ = new ComprehensionCondStep(iter_slot_, accu_slot_,
                                             short_circuiting_, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(cond_step_));
      break;
    }
    case cel::ast_internal::LOOP_STEP: {
      auto jump_to_next = CreateJumpStep(
          next_step_pos_ - visitor_->GetCurrentIndex() - 1, expr->id());
      if (jump_to_next.ok()) {
        visitor_->AddStep(std::move(jump_to_next));
      }
      // Set offsets.
      cond_step_->set_jump_offset(visitor_->GetCurrentIndex() - cond_step_pos_ -
                                  1);
      next_step_->set_jump_offset(visitor_->GetCurrentIndex() - next_step_pos_ -
                                  1);
      break;
    }
    case cel::ast_internal::RESULT: {
      visitor_->AddStep(CreateComprehensionFinishStep(accu_slot_, expr->id()));
      next_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        next_step_pos_ - 1);
      cond_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        cond_step_pos_ - 1);
      break;
    }
  }
}

void ComprehensionVisitor::PostVisitArgTrivial(
    cel::ast_internal::ComprehensionArg arg_num,
    const cel::ast_internal::Expr* expr) {
  switch (arg_num) {
    case cel::ast_internal::ITER_RANGE: {
      break;
    }
    case cel::ast_internal::ACCU_INIT: {
      if (!accu_init_extracted_) {
        visitor_->AddStep(CreateAssignSlotAndPopStep(accu_slot_));
      }
      break;
    }
    case cel::ast_internal::LOOP_CONDITION: {
      break;
    }
    case cel::ast_internal::LOOP_STEP: {
      break;
    }
    case cel::ast_internal::RESULT: {
      visitor_->AddStep(CreateClearSlotStep(accu_slot_, expr->id()));
      break;
    }
  }
}

void ComprehensionVisitor::PostVisit(const cel::ast_internal::Expr* expr) {}

// Flattens the expression table into the end of the mainline expression vector
// and returns an index to the individual sub expressions.
std::vector<ExecutionPathView> FlattenExpressionTable(
    ExpressionTable& expression_table, ExecutionPath& main) {
  std::vector<std::pair<size_t, size_t>> ranges;
  ranges.push_back(std::make_pair(0, main.size()));

  for (int i = 0; i < expression_table.size(); ++i) {
    ExecutionPath& subexpression = expression_table[i];
    ranges.push_back(std::make_pair(main.size(), subexpression.size()));
    absl::c_move(subexpression, std::back_inserter(main));
  }
  expression_table.clear();
  // the main program is now stable, can make the indexes.
  std::vector<ExecutionPathView> subexpressions;
  subexpressions.reserve(ranges.size());
  for (const auto& range : ranges) {
    subexpressions.push_back(
        absl::MakeSpan(main).subspan(range.first, range.second));
  }
  return subexpressions;
}

}  // namespace

absl::StatusOr<FlatExpression> FlatExprBuilder::CreateExpressionImpl(
    std::unique_ptr<Ast> ast, std::vector<RuntimeIssue>* issues) const {
  ExecutionPath execution_path;
  ExpressionTable expression_table;

  // These objects are expected to remain scoped to one build call -- references
  // to them shouldn't be persisted in any part of the result expression.
  TypeFactory type_factory(cel::MemoryManagerRef::ReferenceCounting());
  TypeManager type_manager(type_factory,
                           type_registry_.GetComposedTypeProvider());
  ValueManager value_factory(type_manager);

  RuntimeIssue::Severity max_severity = options_.fail_on_warnings
                                            ? RuntimeIssue::Severity::kWarning
                                            : RuntimeIssue::Severity::kError;
  IssueCollector issue_collector(max_severity);
  Resolver resolver(container_, function_registry_, type_registry_,
                    value_factory, type_registry_.resolveable_enums(),
                    options_.enable_qualified_type_identifiers);

  PlannerContext::ProgramTree program_tree;
  PlannerContext extension_context(resolver, options_, value_factory,
                                   issue_collector, execution_path,
                                   program_tree);

  auto& ast_impl = AstImpl::CastFromPublicAst(*ast);

  if (absl::StartsWith(container_, ".") || absl::EndsWith(container_, ".")) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid expression container: '", container_, "'"));
  }

  for (const std::unique_ptr<AstTransform>& transform : ast_transforms_) {
    CEL_RETURN_IF_ERROR(transform->UpdateAst(extension_context, ast_impl));
  }

  std::vector<std::unique_ptr<ProgramOptimizer>> optimizers;
  for (const ProgramOptimizerFactory& optimizer_factory : program_optimizers_) {
    CEL_ASSIGN_OR_RETURN(optimizers.emplace_back(),
                         optimizer_factory(extension_context, ast_impl));
  }

  FlatExprVisitor visitor(resolver, options_, std::move(optimizers),
                          ast_impl.reference_map(), expression_table,
                          execution_path, value_factory, issue_collector,
                          program_tree, extension_context);

  cel::ast_internal::TraversalOptions opts;
  opts.use_comprehension_callbacks = true;
  AstTraverse(&ast_impl.root_expr(), &ast_impl.source_info(), &visitor, opts);

  if (!visitor.progress_status().ok()) {
    return visitor.progress_status();
  }

  if (issues != nullptr) {
    (*issues) = issue_collector.ExtractIssues();
  }

  std::vector<ExecutionPathView> subexpressions =
      FlattenExpressionTable(expression_table, execution_path);

  return FlatExpression(std::move(execution_path), std::move(subexpressions),
                        visitor.slot_count(),
                        type_registry_.GetComposedTypeProvider(), options_);
}

}  // namespace google::api::expr::runtime
