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

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/ast.h"
#include "base/ast_internal.h"
#include "base/internal/ast_impl.h"
#include "base/values/string_value.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/comprehension_step.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/container_access_step.h"
#include "eval/eval/create_list_step.h"
#include "eval/eval/create_struct_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/eval/function_step.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/jump_step.h"
#include "eval/eval/logic_step.h"
#include "eval/eval/regex_match_step.h"
#include "eval/eval/select_step.h"
#include "eval/eval/shadowable_value_step.h"
#include "eval/eval/ternary_step.h"
#include "eval/internal/interop.h"
#include "eval/public/ast_traverse_native.h"
#include "eval/public/ast_visitor_native.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/source_position.h"
#include "eval/public/source_position_native.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Handle;
using ::cel::StringValue;
using ::cel::Value;
using ::cel::ast::Ast;
using ::cel::ast::internal::AstImpl;
using ::cel::interop_internal::CreateIntValue;
using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::SourceInfo;

using Ident = ::google::api::expr::v1alpha1::Expr::Ident;
using Select = ::google::api::expr::v1alpha1::Expr::Select;
using Call = ::google::api::expr::v1alpha1::Expr::Call;
using CreateList = ::google::api::expr::v1alpha1::Expr::CreateList;
using CreateStruct = ::google::api::expr::v1alpha1::Expr::CreateStruct;
using Comprehension = ::google::api::expr::v1alpha1::Expr::Comprehension;

constexpr int64_t kExprIdNotFromAst = -1;

template <typename ExprT>
bool IsFunctionOverload(
    const ExprT& expr, absl::string_view function, absl::string_view overload,
    size_t arity,
    const absl::flat_hash_map<int64_t, cel::ast::internal::Reference>*
        reference_map) {
  if (reference_map == nullptr || !expr.has_call_expr()) {
    return false;
  }
  const auto& call_expr = expr.call_expr();
  if (call_expr.function() != function) {
    return false;
  }
  if (call_expr.args().size() + (call_expr.has_target() ? 1 : 0) != arity) {
    return false;
  }
  auto reference = reference_map->find(expr.id());
  if (reference != reference_map->end() &&
      reference->second.overload_id().size() == 1 &&
      reference->second.overload_id().front() == overload) {
    return true;
  }
  return false;
}

// Forward declare to resolve circular dependency for short_circuiting visitors.
class FlatExprVisitor;

// Abstraction for deduplicating regular expressions over the course of a single
// create expression call. Should not be used during evaluation. Uses
// std::shared_ptr and std::weak_ptr.
class RegexProgramBuilder final {
 public:
  explicit RegexProgramBuilder(int max_program_size)
      : max_program_size_(max_program_size) {}

  absl::StatusOr<std::shared_ptr<const RE2>> BuildRegexProgram(
      std::string pattern) {
    auto existing = programs_.find(pattern);
    if (existing != programs_.end()) {
      if (auto program = existing->second.lock(); program) {
        return program;
      }
      programs_.erase(existing);
    }
    auto program = std::make_shared<RE2>(pattern);
    if (max_program_size_ > 0 && program->ProgramSize() > max_program_size_) {
      return absl::InvalidArgumentError("exceeded RE2 max program size");
    }
    if (!program->ok()) {
      return absl::InvalidArgumentError("invalid_argument");
    }
    programs_.insert({std::move(pattern), program});
    return program;
  }

 private:
  const int max_program_size_;
  absl::flat_hash_map<std::string, std::weak_ptr<const RE2>> programs_;
};

// A convenience wrapper for offset-calculating logic.
class Jump {
 public:
  explicit Jump() : self_index_(-1), jump_step_(nullptr) {}
  explicit Jump(int self_index,
                google::api::expr::runtime::JumpStepBase* jump_step)
      : self_index_(self_index), jump_step_(jump_step) {}
  void set_target(int index) {
    // 0 offset means no-op.
    jump_step_->set_jump_offset(index - self_index_ - 1);
  }
  bool exists() { return jump_step_ != nullptr; }

 private:
  int self_index_;
  google::api::expr::runtime::JumpStepBase* jump_step_;
};

class CondVisitor {
 public:
  virtual ~CondVisitor() = default;
  virtual void PreVisit(const cel::ast::internal::Expr* expr) = 0;
  virtual void PostVisitArg(int arg_num,
                            const cel::ast::internal::Expr* expr) = 0;
  virtual void PostVisit(const cel::ast::internal::Expr* expr) = 0;
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

  void PreVisit(const cel::ast::internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast::internal::Expr* expr) override;
  void PostVisit(const cel::ast::internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  const bool cond_value_;
  Jump jump_step_;
  bool short_circuiting_;
};

class TernaryCondVisitor : public CondVisitor {
 public:
  explicit TernaryCondVisitor(FlatExprVisitor* visitor) : visitor_(visitor) {}

  void PreVisit(const cel::ast::internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast::internal::Expr* expr) override;
  void PostVisit(const cel::ast::internal::Expr* expr) override;

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

  void PreVisit(const cel::ast::internal::Expr* expr) override;
  void PostVisitArg(int arg_num,
                    const cel::ast::internal::Expr* expr) override {}
  void PostVisit(const cel::ast::internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
};

// Visitor Comprehension expression.
class ComprehensionVisitor : public CondVisitor {
 public:
  explicit ComprehensionVisitor(FlatExprVisitor* visitor, bool short_circuiting,
                                bool enable_vulnerability_check)
      : visitor_(visitor),
        next_step_(nullptr),
        cond_step_(nullptr),
        short_circuiting_(short_circuiting),
        enable_vulnerability_check_(enable_vulnerability_check) {}

  void PreVisit(const cel::ast::internal::Expr* expr) override;
  void PostVisitArg(int arg_num, const cel::ast::internal::Expr* expr) override;
  void PostVisit(const cel::ast::internal::Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  google::api::expr::runtime::ComprehensionNextStep* next_step_;
  google::api::expr::runtime::ComprehensionCondStep* cond_step_;
  int next_step_pos_;
  int cond_step_pos_;
  bool short_circuiting_;
  bool enable_vulnerability_check_;
};

class FlatExprVisitor : public cel::ast::internal::AstVisitor {
 public:
  FlatExprVisitor(
      const google::api::expr::runtime::Resolver& resolver,
      const cel::RuntimeOptions& options,
      const absl::flat_hash_map<std::string, Handle<Value>>& constant_idents,
      bool enable_comprehension_vulnerability_check,
      bool enable_regex_precompilation,
      absl::Span<const std::unique_ptr<ProgramOptimizer>> program_optimizers,
      const absl::flat_hash_map<int64_t, cel::ast::internal::Reference>*
          reference_map,
      google::api::expr::runtime::ExecutionPath* path,
      google::api::expr::runtime::BuilderWarnings* warnings,
      google::protobuf::Arena* arena, PlannerContext::ProgramTree& program_tree,
      PlannerContext& extension_context)
      : resolver_(resolver),
        execution_path_(path),
        progress_status_(absl::OkStatus()),
        resolved_select_expr_(nullptr),
        parent_expr_(nullptr),
        options_(options),
        constant_idents_(constant_idents),
        enable_comprehension_vulnerability_check_(
            enable_comprehension_vulnerability_check),
        enable_regex_precompilation_(enable_regex_precompilation),
        program_optimizers_(program_optimizers),
        builder_warnings_(warnings),
        regex_program_builder_(options_.regex_max_program_size),
        reference_map_(reference_map),
        arena_(arena),
        program_tree_(program_tree),
        extension_context_(extension_context) {}

  void PreVisitExpr(const cel::ast::internal::Expr* expr,
                    const cel::ast::internal::SourcePosition*) override {
    ValidateOrError(
        !absl::holds_alternative<absl::monostate>(expr->expr_kind()),
        "Invalid empty expression");
    if (!progress_status_.ok()) {
      return;
    }
    if (program_optimizers_.empty()) {
      return;
    }
    PlannerContext::ProgramInfo& info = program_tree_[expr];
    info.range_start = execution_path_->size();
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

  void PostVisitExpr(const cel::ast::internal::Expr* expr,
                     const cel::ast::internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    // TODO(issues/5): this will be generalized later.
    if (program_optimizers_.empty()) {
      return;
    }
    PlannerContext::ProgramInfo& info = program_tree_[expr];
    info.range_len = execution_path_->size() - info.range_start;
    parent_expr_ = info.parent;

    for (const std::unique_ptr<ProgramOptimizer>& optimizer :
         program_optimizers_) {
      absl::Status status = optimizer->OnPostVisit(extension_context_, *expr);
      if (!status.ok()) {
        SetProgressStatusError(status);
      }
    }
  }

  void PostVisitConst(const cel::ast::internal::Constant* const_expr,
                      const cel::ast::internal::Expr* expr,
                      const cel::ast::internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    AddStep(CreateConstValueStep(*const_expr, expr->id()));
  }

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const cel::ast::internal::Ident* ident_expr,
                      const cel::ast::internal::Expr* expr,
                      const cel::ast::internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    const std::string& path = ident_expr->name();
    if (!ValidateOrError(
            !path.empty(),
            "Invalid expression: identifier 'name' must not be empty")) {
      return;
    }

    // Automatically replace constant idents with the backing CEL values.
    auto constant = constant_idents_.find(path);
    if (constant != constant_idents_.end()) {
      AddStep(CreateConstValueStep(constant->second, expr->id(), false));
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

    AddStep(
        google::api::expr::runtime::CreateIdentStep(*ident_expr, expr->id()));
  }

  void PreVisitSelect(const cel::ast::internal::Select* select_expr,
                      const cel::ast::internal::Expr* expr,
                      const cel::ast::internal::SourcePosition*) override {
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
  void PostVisitSelect(const cel::ast::internal::Select* select_expr,
                       const cel::ast::internal::Expr* expr,
                       const cel::ast::internal::SourcePosition*) override {
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
                             options_.enable_empty_wrapper_null_unboxing));
  }

  // Call node handler group.
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const cel::ast::internal::Call* call_expr,
                    const cel::ast::internal::Expr* expr,
                    const cel::ast::internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    std::unique_ptr<CondVisitor> cond_visitor;
    if (call_expr->function() == google::api::expr::runtime::builtin::kAnd) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ false, options_.short_circuiting);
    } else if (call_expr->function() ==
               google::api::expr::runtime::builtin::kOr) {
      cond_visitor = std::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ true, options_.short_circuiting);
    } else if (call_expr->function() ==
               google::api::expr::runtime::builtin::kTernary) {
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
  void PostVisitCall(const cel::ast::internal::Call* call_expr,
                     const cel::ast::internal::Expr* expr,
                     const cel::ast::internal::SourcePosition*) override {
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
    if (call_expr->function() == google::api::expr::runtime::builtin::kIndex) {
      AddStep(CreateContainerAccessStep(*call_expr, expr->id()));
      return;
    }

    // Establish the search criteria for a given function.
    absl::string_view function = call_expr->function();
    bool receiver_style = call_expr->has_target();
    size_t num_args = call_expr->args().size() + (receiver_style ? 1 : 0);
    auto arguments_matcher = ArgumentsMatcher(num_args);

    // Check to see if this is regular expression matching and the pattern is a
    // constant.
    if (options_.enable_regex && enable_regex_precompilation_ &&
        IsOptimizeableMatchesCall(*expr, *call_expr)) {
      auto program = regex_program_builder_.BuildRegexProgram(
          GetConstantString(call_expr->args().back()));
      if (!program.ok()) {
        SetProgressStatusError(program.status());
        return;
      }
      AddStep(CreateRegexMatchStep(std::move(program).value(), expr->id()));
      return;
    }

    // Check to see if this is a special case of add that should really be
    // treated as a list append
    if (options_.enable_comprehension_list_append &&
        call_expr->function() == google::api::expr::runtime::builtin::kAdd &&
        call_expr->args().size() == 2 && !comprehension_stack_.empty()) {
      const cel::ast::internal::Comprehension* comprehension =
          comprehension_stack_.top();
      absl::string_view accu_var = comprehension->accu_var();
      if (comprehension->accu_init().has_list_expr() &&
          call_expr->args()[0].has_ident_expr() &&
          call_expr->args()[0].ident_expr().name() == accu_var) {
        const cel::ast::internal::Expr& loop_step = comprehension->loop_step();
        // Macro loop_step for a map() will contain a list concat operation:
        //   accu_var + [elem]
        if (&loop_step == expr) {
          function = google::api::expr::runtime::builtin::kRuntimeListAppend;
        }
        // Macro loop_step for a filter() will contain a ternary:
        //   filter ? result + [elem] : result
        if (loop_step.has_call_expr() &&
            loop_step.call_expr().function() ==
                google::api::expr::runtime::builtin::kTernary &&
            loop_step.call_expr().args().size() == 3 &&
            &(loop_step.call_expr().args()[1]) == expr) {
          function = google::api::expr::runtime::builtin::kRuntimeListAppend;
        }
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
      auto status = builder_warnings_->AddWarning(absl::InvalidArgumentError(
          "No overloads provided for FunctionStep creation"));
      if (!status.ok()) {
        SetProgressStatusError(status);
        return;
      }
    }
    AddStep(CreateFunctionStep(*call_expr, expr->id(), std::move(overloads)));
  }

  void PreVisitComprehension(
      const cel::ast::internal::Comprehension* comprehension,
      const cel::ast::internal::Expr* expr,
      const cel::ast::internal::SourcePosition*) override {
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
    comprehension_stack_.push(comprehension);
    cond_visitor_stack_.push(
        {expr, std::make_unique<ComprehensionVisitor>(
                   this, options_.short_circuiting,
                   enable_comprehension_vulnerability_check_)});
    auto cond_visitor = FindCondVisitor(expr);
    cond_visitor->PreVisit(expr);
  }

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(
      const cel::ast::internal::Comprehension* comprehension_expr,
      const cel::ast::internal::Expr* expr,
      const cel::ast::internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    comprehension_stack_.pop();

    auto cond_visitor = FindCondVisitor(expr);
    cond_visitor->PostVisit(expr);
    cond_visitor_stack_.pop();
  }

  // Invoked after each argument node processed.
  void PostVisitArg(int arg_num, const cel::ast::internal::Expr* expr,
                    const cel::ast::internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    auto cond_visitor = FindCondVisitor(expr);
    if (cond_visitor) {
      cond_visitor->PostVisitArg(arg_num, expr);
    }
  }

  // Nothing to do.
  void PostVisitTarget(const cel::ast::internal::Expr* expr,
                       const cel::ast::internal::SourcePosition*) override {}

  // CreateList node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateList(const cel::ast::internal::CreateList* list_expr,
                           const cel::ast::internal::Expr* expr,
                           const cel::ast::internal::SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (options_.enable_comprehension_list_append &&
        !comprehension_stack_.empty() &&
        &(comprehension_stack_.top()->accu_init()) == expr) {
      AddStep(CreateCreateMutableListStep(*list_expr, expr->id()));
      return;
    }
    AddStep(CreateCreateListStep(*list_expr, expr->id()));
  }

  // CreateStruct node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateStruct(
      const cel::ast::internal::CreateStruct* struct_expr,
      const cel::ast::internal::Expr* expr,
      const cel::ast::internal::SourcePosition*) override {
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
      AddStep(CreateCreateStructStep(*struct_expr, expr->id()));
      return;
    }

    // If the message name is not empty, then the message name must be resolved
    // within the container, and if a descriptor is found, then a proto message
    // creation step will be created.
    auto type_adapter = resolver_.FindTypeAdapter(message_name, expr->id());
    if (ValidateOrError(type_adapter.has_value() &&
                            type_adapter->mutation_apis() != nullptr,
                        "Invalid struct creation: missing type info for '",
                        message_name, "'")) {
      for (const auto& entry : struct_expr->entries()) {
        ValidateOrError(entry.has_field_key(),
                        "Struct entry missing field name");
        ValidateOrError(entry.has_value(), "Struct entry missing value");
      }
      AddStep(CreateCreateStructStep(
          *struct_expr, type_adapter->mutation_apis(), expr->id()));
    }
  }

  absl::Status progress_status() const { return progress_status_; }

  void AddStep(absl::StatusOr<
               std::unique_ptr<google::api::expr::runtime::ExpressionStep>>
                   step) {
    if (step.ok() && progress_status_.ok()) {
      execution_path_->push_back(*std::move(step));
    } else {
      SetProgressStatusError(step.status());
    }
  }

  void AddStep(
      std::unique_ptr<google::api::expr::runtime::ExpressionStep> step) {
    if (progress_status_.ok()) {
      execution_path_->push_back(std::move(step));
    }
  }

  void SetProgressStatusError(const absl::Status& status) {
    if (progress_status_.ok() && !status.ok()) {
      progress_status_ = status;
    }
  }

  // Index of the next step to be inserted.
  int GetCurrentIndex() const { return execution_path_->size(); }

  CondVisitor* FindCondVisitor(const cel::ast::internal::Expr* expr) const {
    if (cond_visitor_stack_.empty()) {
      return nullptr;
    }

    const auto& latest = cond_visitor_stack_.top();

    return (latest.first == expr) ? latest.second.get() : nullptr;
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
  bool IsConstantString(const cel::ast::internal::Expr& expr) const {
    if (expr.has_const_expr() && expr.const_expr().has_string_value()) {
      return true;
    }
    if (!expr.has_ident_expr()) {
      return false;
    }
    auto const_value = constant_idents_.find(expr.ident_expr().name());
    return const_value != constant_idents_.end() &&
           const_value->second->Is<StringValue>();
  }

  std::string GetConstantString(const cel::ast::internal::Expr& expr) const {
    ABSL_ASSERT(IsConstantString(expr));
    if (expr.has_const_expr()) {
      return expr.const_expr().string_value();
    }
    return constant_idents_.find(expr.ident_expr().name())
        ->second.As<StringValue>()
        ->ToString();
  }

  bool IsOptimizeableMatchesCall(
      const cel::ast::internal::Expr& expr,
      const cel::ast::internal::Call& call_expr) const {
    return IsFunctionOverload(expr,
                              google::api::expr::runtime::builtin::kRegexMatch,
                              "matches_string", 2, reference_map_) &&
           IsConstantString(call_expr.args().back());
  }

  const google::api::expr::runtime::Resolver& resolver_;
  google::api::expr::runtime::ExecutionPath* execution_path_;
  absl::Status progress_status_;

  std::stack<
      std::pair<const cel::ast::internal::Expr*, std::unique_ptr<CondVisitor>>>
      cond_visitor_stack_;

  // Maps effective namespace names to Expr objects (IDENTs/SELECTs) that
  // define scopes for those namespaces.
  std::unordered_map<const cel::ast::internal::Expr*, std::string>
      namespace_map_;
  // Tracks SELECT-...SELECT-IDENT chains.
  std::deque<std::pair<const cel::ast::internal::Expr*, std::string>>
      namespace_stack_;

  // When multiple SELECT-...SELECT-IDENT chain is resolved as namespace, this
  // field is used as marker suppressing CelExpression creation for SELECTs.
  const cel::ast::internal::Expr* resolved_select_expr_;

  // Used for assembling a temporary tree mapping program segments
  // to source expr nodes.
  const cel::ast::internal::Expr* parent_expr_;

  const cel::RuntimeOptions& options_;

  const absl::flat_hash_map<std::string, Handle<Value>>& constant_idents_;

  std::stack<const cel::ast::internal::Comprehension*> comprehension_stack_;

  bool enable_comprehension_vulnerability_check_;
  bool enable_regex_precompilation_;

  absl::Span<const std::unique_ptr<ProgramOptimizer>> program_optimizers_;
  google::api::expr::runtime::BuilderWarnings* builder_warnings_;

  RegexProgramBuilder regex_program_builder_;
  const absl::flat_hash_map<int64_t, cel::ast::internal::Reference>* const
      reference_map_;

  google::protobuf::Arena* const arena_;
  PlannerContext::ProgramTree& program_tree_;
  PlannerContext extension_context_;
};

void BinaryCondVisitor::PreVisit(const cel::ast::internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 2,
      "Invalid argument count for a binary function call.");
}

void BinaryCondVisitor::PostVisitArg(int arg_num,
                                     const cel::ast::internal::Expr* expr) {
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

void BinaryCondVisitor::PostVisit(const cel::ast::internal::Expr* expr) {
  visitor_->AddStep((cond_value_) ? CreateOrStep(expr->id())
                                  : CreateAndStep(expr->id()));
  if (short_circuiting_) {
    // If shortcircuiting is enabled, point the conditional jump past the
    // boolean operator step.
    jump_step_.set_target(visitor_->GetCurrentIndex());
  }
}

void TernaryCondVisitor::PreVisit(const cel::ast::internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 3,
      "Invalid argument count for a ternary function call.");
}

void TernaryCondVisitor::PostVisitArg(int arg_num,
                                      const cel::ast::internal::Expr* expr) {
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

void TernaryCondVisitor::PostVisit(const cel::ast::internal::Expr*) {
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
    const cel::ast::internal::Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args().size() == 3,
      "Invalid argument count for a ternary function call.");
}

void ExhaustiveTernaryCondVisitor::PostVisit(
    const cel::ast::internal::Expr* expr) {
  visitor_->AddStep(CreateTernaryStep(expr->id()));
}

// ComprehensionAccumulationReferences recursively walks an expression to count
// the locations where the given accumulation var_name is referenced.
//
// The purpose of this function is to detect cases where the accumulation
// variable might be used in hand-rolled ASTs that cause exponential memory
// consumption. The var_name is generally not accessible by CEL expression
// writers, only by macro authors. However, a hand-rolled AST makes it possible
// to misuse the accumulation variable.
//
// Limitations:
// - This check only covers standard operators and functions.
//   Extension functions may cause the same issue if they allocate an amount of
//   memory that is dependent on the size of the inputs.
//
// - This check is not exhaustive. There may be ways to construct an AST to
//   trigger exponential memory growth not captured by this check.
//
// The algorithm for reference counting is as follows:
//
//  * Calls - If the call is a concatenation operator, sum the number of places
//            where the variable appears within the call, as this could result
//            in memory explosion if the accumulation variable type is a list
//            or string. Otherwise, return 0.
//
//            accu: ["hello"]
//            expr: accu + accu // memory grows exponentionally
//
//  * CreateList - If the accumulation var_name appears within multiple elements
//            of a CreateList call, this means that the accumulation is
//            generating an ever-expanding tree of values that will likely
//            exhaust memory.
//
//            accu: ["hello"]
//            expr: [accu, accu] // memory grows exponentially
//
//  * CreateStruct - If the accumulation var_name as an entry within the
//            creation of a map or message value, then it's possible that the
//            comprehension is accumulating an ever-expanding tree of values.
//
//            accu: {"key": "val"}
//            expr: {1: accu, 2: accu}
//
//  * Comprehension - If the accumulation var_name is not shadowed by a nested
//            iter_var or accu_var, then it may be accmulating memory within a
//            nested context. The accumulation may occur on either the
//            comprehension loop_step or result step.
//
// Since this behavior generally only occurs within hand-rolled ASTs, it is
// very reasonable to opt-in to this check only when using human authored ASTs.
int ComprehensionAccumulationReferences(const cel::ast::internal::Expr& expr,
                                        absl::string_view var_name) {
  struct Handler {
    const cel::ast::internal::Expr& expr;
    absl::string_view var_name;

    int operator()(const cel::ast::internal::Call& call) {
      int references = 0;
      absl::string_view function = call.function();
      // Return the maximum reference count of each side of the ternary branch.
      if (function == google::api::expr::runtime::builtin::kTernary &&
          call.args().size() == 3) {
        return std::max(
            ComprehensionAccumulationReferences(call.args()[1], var_name),
            ComprehensionAccumulationReferences(call.args()[2], var_name));
      }
      // Return the number of times the accumulator var_name appears in the add
      // expression. There's no arg size check on the add as it may become a
      // variadic add at a future date.
      if (function == google::api::expr::runtime::builtin::kAdd) {
        for (int i = 0; i < call.args().size(); i++) {
          references +=
              ComprehensionAccumulationReferences(call.args()[i], var_name);
        }

        return references;
      }
      // Return whether the accumulator var_name is used as the operand in an
      // index expression or in the identity `dyn` function.
      if ((function == google::api::expr::runtime::builtin::kIndex &&
           call.args().size() == 2) ||
          (function == google::api::expr::runtime::builtin::kDyn &&
           call.args().size() == 1)) {
        return ComprehensionAccumulationReferences(call.args()[0], var_name);
      }
      return 0;
    }
    int operator()(const cel::ast::internal::Comprehension& comprehension) {
      absl::string_view accu_var = comprehension.accu_var();
      absl::string_view iter_var = comprehension.iter_var();

      int result_references = 0;
      int loop_step_references = 0;
      int sum_of_accumulator_references = 0;

      // The accumulation or iteration variable shadows the var_name and so will
      // not manipulate the target var_name in a nested comprehension scope.
      if (accu_var != var_name && iter_var != var_name) {
        loop_step_references = ComprehensionAccumulationReferences(
            comprehension.loop_step(), var_name);
      }

      // Accumulator variable (but not necessarily iter var) can shadow an
      // outer accumulator variable in the result sub-expression.
      if (accu_var != var_name) {
        result_references = ComprehensionAccumulationReferences(
            comprehension.result(), var_name);
      }

      // Count the raw number of times the accumulator variable was referenced.
      // This is to account for cases where the outer accumulator is shadowed by
      // the inner accumulator, while the inner accumulator is being used as the
      // iterable range.
      //
      // An equivalent expression to this problem:
      //
      // outer_accu := outer_accu
      // for y in outer_accu:
      //     outer_accu += input
      // return outer_accu

      // If this is overly restrictive (Ex: when generalized reducers is
      // implemented), we may need to revisit this solution

      sum_of_accumulator_references = ComprehensionAccumulationReferences(
          comprehension.accu_init(), var_name);

      sum_of_accumulator_references += ComprehensionAccumulationReferences(
          comprehension.iter_range(), var_name);

      // Count the number of times the accumulator var_name within the loop_step
      // or the nested comprehension result.
      //
      // This doesn't cover cases where the inner accumulator accumulates the
      // outer accumulator then is returned in the inner comprehension result.
      return std::max({loop_step_references, result_references,
                       sum_of_accumulator_references});
    }

    int operator()(const cel::ast::internal::CreateList& list) {
      // Count the number of times the accumulator var_name appears within a
      // create list expression's elements.
      int references = 0;
      for (int i = 0; i < list.elements().size(); i++) {
        references +=
            ComprehensionAccumulationReferences(list.elements()[i], var_name);
      }
      return references;
    }

    int operator()(const cel::ast::internal::CreateStruct& map) {
      // Count the number of times the accumulation variable occurs within
      // entry values.
      int references = 0;
      for (int i = 0; i < map.entries().size(); i++) {
        const auto& entry = map.entries()[i];
        if (entry.has_value()) {
          references +=
              ComprehensionAccumulationReferences(entry.value(), var_name);
        }
      }
      return references;
    }

    int operator()(const cel::ast::internal::Select& select) {
      // Test only expressions have a boolean return and thus cannot easily
      // allocate large amounts of memory.
      if (select.test_only()) {
        return 0;
      }
      // Return whether the accumulator var_name appears within a non-test
      // select operand.
      return ComprehensionAccumulationReferences(select.operand(), var_name);
    }

    int operator()(const cel::ast::internal::Ident& ident) {
      // Return whether the identifier name equals the accumulator var_name.
      return ident.name() == var_name ? 1 : 0;
    }

    int operator()(const cel::ast::internal::Constant& constant) { return 0; }

    int operator()(absl::monostate) { return 0; }
  } handler{expr, var_name};
  return absl::visit(handler, expr.expr_kind());
}

void ComprehensionVisitor::PreVisit(const cel::ast::internal::Expr*) {
  constexpr int64_t kLoopStepPlaceholder = -10;
  visitor_->AddStep(CreateConstValueStep(CreateIntValue(kLoopStepPlaceholder),
                                         kExprIdNotFromAst, false));
}

void ComprehensionVisitor::PostVisitArg(int arg_num,
                                        const cel::ast::internal::Expr* expr) {
  const auto* comprehension = &expr->comprehension_expr();
  const auto& accu_var = comprehension->accu_var();
  const auto& iter_var = comprehension->iter_var();
  // TODO(issues/20): Consider refactoring the comprehension prologue step.
  switch (arg_num) {
    case cel::ast::internal::ITER_RANGE: {
      // Post-process iter_range to list its keys if it's a map.
      visitor_->AddStep(CreateListKeysStep(expr->id()));
      // Setup index stack position
      visitor_->AddStep(
          CreateConstValueStep(CreateIntValue(-1), kExprIdNotFromAst, false));
      // Element at index.
      constexpr int64_t kCurrentValuePlaceholder = -20;
      visitor_->AddStep(CreateConstValueStep(
          CreateIntValue(kCurrentValuePlaceholder), kExprIdNotFromAst, false));
      break;
    }
    case cel::ast::internal::ACCU_INIT: {
      next_step_pos_ = visitor_->GetCurrentIndex();
      next_step_ = new ComprehensionNextStep(accu_var, iter_var, expr->id());
      visitor_->AddStep(
          std::unique_ptr<google::api::expr::runtime::ExpressionStep>(
              next_step_));
      break;
    }
    case cel::ast::internal::LOOP_CONDITION: {
      cond_step_pos_ = visitor_->GetCurrentIndex();
      cond_step_ = new ComprehensionCondStep(accu_var, iter_var,
                                             short_circuiting_, expr->id());
      visitor_->AddStep(
          std::unique_ptr<google::api::expr::runtime::ExpressionStep>(
              cond_step_));
      break;
    }
    case cel::ast::internal::LOOP_STEP: {
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
    case cel::ast::internal::RESULT: {
      visitor_->AddStep(
          std::unique_ptr<google::api::expr::runtime::ExpressionStep>(
              new ComprehensionFinish(accu_var, iter_var, expr->id())));
      next_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        next_step_pos_ - 1);
      cond_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        cond_step_pos_ - 1);
      break;
    }
  }
}

void ComprehensionVisitor::PostVisit(const cel::ast::internal::Expr* expr) {
  if (enable_vulnerability_check_) {
    const auto* comprehension = &expr->comprehension_expr();
    absl::string_view accu_var = comprehension->accu_var();
    const auto& loop_step = comprehension->loop_step();
    visitor_->ValidateOrError(
        ComprehensionAccumulationReferences(loop_step, accu_var) < 2,
        "Comprehension contains memory exhaustion vulnerability");
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const Expr* expr,
                                  const SourceInfo* source_info,
                                  std::vector<absl::Status>* warnings) const {
  ABSL_ASSERT(expr != nullptr);
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<Ast> converted_ast,
      cel::extensions::CreateAstFromParsedExpr(*expr, source_info));
  return CreateExpressionImpl(*converted_ast, warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const Expr* expr,
                                  const SourceInfo* source_info) const {
  return CreateExpression(expr, source_info,
                          /*warnings=*/nullptr);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const CheckedExpr* checked_expr,
                                  std::vector<absl::Status>* warnings) const {
  ABSL_ASSERT(checked_expr != nullptr);
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<Ast> converted_ast,
      cel::extensions::CreateAstFromCheckedExpr(*checked_expr));
  return CreateExpressionImpl(*converted_ast, warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const CheckedExpr* checked_expr) const {
  return CreateExpression(checked_expr, /*warnings=*/nullptr);
}

// TODO(issues/5): move ast conversion to client responsibility and
// update pre-processing steps to work without mutating the input AST.
absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpressionImpl(
    cel::ast::Ast& ast, std::vector<absl::Status>* warnings) const {
  ExecutionPath execution_path;
  BuilderWarnings warnings_builder(options_.fail_on_warnings);
  Resolver resolver(container(), GetRegistry()->InternalGetRegistry(),
                    GetTypeRegistry(),
                    options_.enable_qualified_type_identifiers);
  absl::flat_hash_map<std::string, Handle<Value>> constant_idents;

  PlannerContext::ProgramTree program_tree;
  PlannerContext extension_context(resolver, *GetTypeRegistry(), options_,
                                   warnings_builder, execution_path,
                                   program_tree);

  auto& ast_impl = AstImpl::CastFromPublicAst(ast);
  const cel::ast::internal::Expr* effective_expr = &ast_impl.root_expr();

  if (absl::StartsWith(container(), ".") || absl::EndsWith(container(), ".")) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid expression container: '", container(), "'"));
  }

  for (const std::unique_ptr<AstTransform>& transform : ast_transforms_) {
    CEL_RETURN_IF_ERROR(transform->UpdateAst(extension_context, ast_impl));
  }

  cel::ast::internal::Expr const_fold_buffer;
  if (constant_folding_) {
    cel::ast::internal::FoldConstants(
        ast_impl.root_expr(), this->GetRegistry()->InternalGetRegistry(),
        constant_arena_, constant_idents, const_fold_buffer);
    effective_expr = &const_fold_buffer;
  }

  auto arena = std::make_unique<google::protobuf::Arena>();

  std::vector<std::unique_ptr<ProgramOptimizer>> optimizers;
  for (const ProgramOptimizerFactory& optimizer_factory : program_optimizers_) {
    CEL_ASSIGN_OR_RETURN(optimizers.emplace_back(),
                         optimizer_factory(extension_context, ast_impl));
  }
  FlatExprVisitor visitor(
      resolver, options_, constant_idents,
      enable_comprehension_vulnerability_check_, enable_regex_precompilation_,
      optimizers, &ast_impl.reference_map(), &execution_path, &warnings_builder,
      arena.get(), program_tree, extension_context);

  AstTraverse(effective_expr, &ast_impl.source_info(), &visitor);

  if (!visitor.progress_status().ok()) {
    return visitor.progress_status();
  }

  if (arena->SpaceUsed() == 0) {
    // No space in the arena was used, delete it.
    arena.reset();
  }

  std::unique_ptr<CelExpression> expression_impl =
      std::make_unique<CelExpressionFlatImpl>(std::move(execution_path),
                                              GetTypeRegistry(), options_,
                                              std::move(arena));

  if (warnings != nullptr) {
    *warnings = std::move(warnings_builder).warnings();
  }
  return expression_impl;
}

}  // namespace google::api::expr::runtime
