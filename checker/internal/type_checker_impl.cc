// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/type_checker_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "checker/internal/namespace_generator.h"
#include "checker/internal/type_check_env.h"
#include "checker/type_check_issue.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/ast_rewrite.h"
#include "common/ast_traverse.h"
#include "common/ast_visitor.h"
#include "common/ast_visitor_base.h"
#include "common/decl.h"
#include "common/expr.h"
#include "common/source.h"
#include "common/type.h"
#include "internal/status_macros.h"

namespace cel::checker_internal {
namespace {

using cel::ast_internal::AstImpl;

using AstType = cel::ast_internal::Type;
using Severity = TypeCheckIssue::Severity;

std::string FormatCandidate(absl::Span<const std::string> qualifiers) {
  return absl::StrJoin(qualifiers, ".");
}

SourceLocation ComputeSourceLocation(const AstImpl& ast, int64_t expr_id) {
  const auto& source_info = ast.source_info();
  auto iter = source_info.positions().find(expr_id);
  if (iter == source_info.positions().end()) {
    return SourceLocation{};
  }
  int32_t absolute_position = iter->second;
  int32_t line_idx = -1;
  for (int32_t offset : source_info.line_offsets()) {
    if (absolute_position >= offset) {
      break;
    }
    ++line_idx;
  }

  if (line_idx < 0 || line_idx >= source_info.line_offsets().size()) {
    return SourceLocation{1, absolute_position};
  }

  int32_t rel_position =
      absolute_position - source_info.line_offsets()[line_idx] + 1;

  return SourceLocation{line_idx + 1, rel_position};
}

class ResolveVisitor : public AstVisitorBase {
 public:
  struct FunctionResolution {
    const FunctionDecl* decl;
    bool namespace_rewrite;
  };

  ResolveVisitor(absl::string_view container,
                 NamespaceGenerator namespace_generator,
                 const TypeCheckEnv& env, const AstImpl& ast,
                 std::vector<TypeCheckIssue>& issues)
      : container_(container),
        namespace_generator_(std::move(namespace_generator)),
        env_(&env),
        issues_(&issues),
        ast_(&ast),
        root_scope_(env.MakeVariableScope()),
        current_scope_(&root_scope_) {}

  void PreVisitExpr(const Expr& expr) override { expr_stack_.push_back(&expr); }

  void PostVisitExpr(const Expr& expr) override {
    if (expr_stack_.empty()) {
      return;
    }
    expr_stack_.pop_back();
  }

  void PreVisitComprehension(const Expr& expr,
                             const ComprehensionExpr& comprehension) override;

  void PostVisitComprehension(const Expr& expr,
                              const ComprehensionExpr& comprehension) override;

  void PreVisitComprehensionSubexpression(
      const Expr& expr, const ComprehensionExpr& comprehension,
      ComprehensionArg comprehension_arg) override;

  void PostVisitComprehensionSubexpression(
      const Expr& expr, const ComprehensionExpr& comprehension,
      ComprehensionArg comprehension_arg) override;

  void PostVisitIdent(const Expr& expr, const IdentExpr& ident) override;

  void PostVisitSelect(const Expr& expr, const SelectExpr& select) override;

  void PostVisitCall(const Expr& expr, const CallExpr& call) override;

  void PostVisitStruct(const Expr& expr,
                       const StructExpr& create_struct) override {
    // TODO: For now, skip resolving create struct type. To allow
    // checking other behaviors. The C++ runtime should still resolve the type
    // based on the runtime configuration.
  }

  // Accessors for resolve values.
  const absl::flat_hash_map<const Expr*, FunctionResolution>& functions()
      const {
    return functions_;
  }

  const absl::flat_hash_map<const Expr*, const VariableDecl*>& attributes()
      const {
    return attributes_;
  }

  const absl::Status& status() const { return status_; }

 private:
  struct ComprehensionScope {
    const Expr* comprehension_expr;
    const VariableScope* parent;
    const VariableScope* accu_scope;
    const VariableScope* iter_scope;
  };

  void ResolveSimpleIdentifier(const Expr& expr, absl::string_view name);

  void ResolveQualifiedIdentifier(const Expr& expr,
                                  absl::Span<const std::string> qualifiers);

  const FunctionDecl* ResolveFunctionCall(const Expr& expr,
                                          absl::string_view function_name,
                                          int arg_count, bool is_receiver);

  void ReportMissingReference(const Expr& expr, absl::string_view name) {
    issues_->push_back(TypeCheckIssue::CreateError(
        ComputeSourceLocation(*ast_, expr.id()),
        absl::StrCat("undeclared reference to '", name, "' (in container '",
                     container_, "')")));
  }

  absl::string_view container_;
  NamespaceGenerator namespace_generator_;
  absl::Nonnull<const TypeCheckEnv*> env_;
  absl::Nonnull<std::vector<TypeCheckIssue>*> issues_;
  absl::Nonnull<const ast_internal::AstImpl*> ast_;
  VariableScope root_scope_;

  // state tracking for the traversal.
  const VariableScope* current_scope_;
  std::vector<const Expr*> expr_stack_;
  absl::flat_hash_map<const Expr*, std::vector<std::string>>
      maybe_namespaced_functions_;
  absl::Status status_;
  std::vector<std::unique_ptr<VariableScope>> comprehension_vars_;
  std::vector<ComprehensionScope> comprehension_scopes_;

  // References that were resolved and may require AST rewrites.
  absl::flat_hash_map<const Expr*, FunctionResolution> functions_;
  absl::flat_hash_map<const Expr*, const VariableDecl*> attributes_;
};

void ResolveVisitor::PostVisitIdent(const Expr& expr, const IdentExpr& ident) {
  if (expr_stack_.size() == 1) {
    ResolveSimpleIdentifier(expr, ident.name());
    return;
  }

  // Walk up the stack to find the qualifiers.
  //
  // If the identifier is the target of a receiver call, then note
  // the function so we can disambiguate namespaced functions later.
  int stack_pos = expr_stack_.size() - 1;
  std::vector<std::string> qualifiers;
  qualifiers.push_back(ident.name());
  const Expr* receiver_call = nullptr;
  const Expr* root_candidate = expr_stack_[stack_pos];

  // Try to identify the root of the select chain, possibly as the receiver of
  // a function call.
  while (stack_pos > 0) {
    --stack_pos;
    const Expr* parent = expr_stack_[stack_pos];

    if (parent->has_call_expr() &&
        (&parent->call_expr().target() == root_candidate)) {
      receiver_call = parent;
      break;
    } else if (!parent->has_select_expr()) {
      break;
    }

    qualifiers.push_back(parent->select_expr().field());
    root_candidate = parent;
  }

  if (receiver_call == nullptr) {
    ResolveQualifiedIdentifier(*root_candidate, qualifiers);
  } else {
    maybe_namespaced_functions_[receiver_call] = std::move(qualifiers);
  }
}

void ResolveVisitor::PostVisitCall(const Expr& expr, const CallExpr& call) {
  if (auto iter = maybe_namespaced_functions_.find(&expr);
      iter != maybe_namespaced_functions_.end()) {
    std::string namespaced_name =
        absl::StrCat(FormatCandidate(iter->second), ".", call.function());
    const FunctionDecl* decl =
        ResolveFunctionCall(expr, namespaced_name, call.args().size(),
                            /* is_receiver= */ false);
    if (decl != nullptr) {
      functions_[&expr] = {decl, /*namespace_rewrite=*/true};
      return;
    }
    // Resolve the target as an attribute (deferred earlier).
    ResolveQualifiedIdentifier(call.target(), iter->second);
  }

  int arg_count = call.args().size();
  if (call.has_target()) {
    ++arg_count;
  }

  const FunctionDecl* decl =
      ResolveFunctionCall(expr, call.function(), arg_count, call.has_target());

  if (decl != nullptr) {
    functions_[&expr] = {decl, /*namespace_rewrite=*/false};
    return;
  }

  ReportMissingReference(expr, call.function());
}

void ResolveVisitor::PreVisitComprehension(
    const Expr& expr, const ComprehensionExpr& comprehension) {
  std::unique_ptr<VariableScope> accu_scope = current_scope_->MakeNestedScope();
  const auto* accu_scope_ptr = accu_scope.get();

  std::unique_ptr<VariableScope> iter_scope = accu_scope->MakeNestedScope();
  const auto* iter_scope_ptr = iter_scope.get();

  iter_scope->InsertVariableIfAbsent(
      MakeVariableDecl(comprehension.iter_var(), DynType()));
  accu_scope->InsertVariableIfAbsent(
      MakeVariableDecl(comprehension.accu_var(), DynType()));

  // Keep the temporary decls alive as long as the visitor.
  comprehension_vars_.push_back(std::move(accu_scope));
  comprehension_vars_.push_back(std::move(iter_scope));

  comprehension_scopes_.push_back(
      {&expr, current_scope_, accu_scope_ptr, iter_scope_ptr});
}

void ResolveVisitor::PostVisitComprehension(
    const Expr& expr, const ComprehensionExpr& comprehension) {
  comprehension_scopes_.pop_back();
}

void ResolveVisitor::PreVisitComprehensionSubexpression(
    const Expr& expr, const ComprehensionExpr& comprehension,
    ComprehensionArg comprehension_arg) {
  if (comprehension_scopes_.empty()) {
    status_.Update(absl::InternalError(
        "Comprehension scope stack is empty in comprehension"));
    return;
  }
  auto& scope = comprehension_scopes_.back();
  if (scope.comprehension_expr != &expr) {
    status_.Update(absl::InternalError("Comprehension scope stack broken"));
    return;
  }
  switch (comprehension_arg) {
    case ComprehensionArg::LOOP_CONDITION:
      current_scope_ = scope.accu_scope;
      break;
    case ComprehensionArg::LOOP_STEP:
      current_scope_ = scope.iter_scope;
      break;
    case ComprehensionArg::RESULT:
      current_scope_ = scope.accu_scope;
      break;
    default:
      current_scope_ = scope.parent;
      break;
  }
}

void ResolveVisitor::PostVisitComprehensionSubexpression(
    const Expr& expr, const ComprehensionExpr& comprehension,
    ComprehensionArg comprehension_arg) {
  if (comprehension_scopes_.empty()) {
    status_.Update(absl::InternalError(
        "Comprehension scope stack is empty in comprehension"));
    return;
  }
  auto& scope = comprehension_scopes_.back();
  if (scope.comprehension_expr != &expr) {
    status_.Update(absl::InternalError("Comprehension scope stack broken"));
    return;
  }
  current_scope_ = scope.parent;
}

const FunctionDecl* ResolveVisitor::ResolveFunctionCall(
    const Expr& expr, absl::string_view function_name, int arg_count,
    bool is_receiver) {
  const FunctionDecl* decl = nullptr;
  namespace_generator_.GenerateCandidates(
      function_name, [&, this](absl::string_view candidate) -> bool {
        decl = env_->LookupFunction(candidate);
        if (decl == nullptr) {
          return true;
        }
        for (const auto& ovl : decl->overloads()) {
          if (ovl.member() == is_receiver && ovl.args().size() == arg_count) {
            return false;
          }
        }
        // Name match, but no matching overloads.
        decl = nullptr;
        return true;
      });
  return decl;
}

void ResolveVisitor::ResolveSimpleIdentifier(const Expr& expr,
                                             absl::string_view name) {
  const VariableDecl* decl = nullptr;
  namespace_generator_.GenerateCandidates(
      name, [&decl, this](absl::string_view candidate) {
        decl = current_scope_->LookupVariable(candidate);
        // continue searching.
        return decl == nullptr;
      });

  if (decl == nullptr) {
    ReportMissingReference(expr, name);
    return;
  }

  attributes_[&expr] = decl;
}

void ResolveVisitor::ResolveQualifiedIdentifier(
    const Expr& expr, absl::Span<const std::string> qualifiers) {
  if (qualifiers.size() == 1) {
    ResolveSimpleIdentifier(expr, qualifiers[0]);
    return;
  }

  absl::Nullable<const VariableDecl*> decl = nullptr;
  int segment_index_out = -1;
  namespace_generator_.GenerateCandidates(
      qualifiers, [&decl, &segment_index_out, this](absl::string_view candidate,
                                                    int segment_index) {
        decl = current_scope_->LookupVariable(candidate);
        if (decl != nullptr) {
          segment_index_out = segment_index;
          return false;
        }
        return true;
      });

  if (decl == nullptr) {
    ReportMissingReference(expr, FormatCandidate(qualifiers));
    return;
  }

  const int num_select_opts = qualifiers.size() - segment_index_out - 1;
  const Expr* root = &expr;
  for (int i = 0; i < num_select_opts; ++i) {
    root = &root->select_expr().operand();
  }

  attributes_[root] = decl;
}

void ResolveVisitor::PostVisitSelect(const Expr& expr,
                                     const SelectExpr& select) {}

class ResolveRewriter : public AstRewriterBase {
 public:
  explicit ResolveRewriter(const ResolveVisitor& visitor,
                           AstImpl::ReferenceMap& references)
      : visitor_(visitor), reference_map_(references) {}
  bool PostVisitRewrite(Expr& expr) override {
    if (auto iter = visitor_.attributes().find(&expr);
        iter != visitor_.attributes().end()) {
      const VariableDecl* decl = iter->second;
      auto& ast_ref = reference_map_[expr.id()];
      ast_ref.set_name(decl->name());
      expr.mutable_ident_expr().set_name(decl->name());
      return true;
    }

    if (auto iter = visitor_.functions().find(&expr);
        iter != visitor_.functions().end()) {
      const FunctionDecl* decl = iter->second.decl;
      const bool needs_rewrite = iter->second.namespace_rewrite;
      auto& ast_ref = reference_map_[expr.id()];
      ast_ref.set_name(decl->name());
      for (const auto& overload : decl->overloads()) {
        // TODO: narrow based on type inferences and shape.
        ast_ref.mutable_overload_id().push_back(overload.id());
      }
      expr.mutable_call_expr().set_function(decl->name());
      if (needs_rewrite && expr.call_expr().has_target()) {
        expr.mutable_call_expr().set_target(nullptr);
      }
      return true;
    }

    return false;
  }

 private:
  const ResolveVisitor& visitor_;
  AstImpl::ReferenceMap& reference_map_;
};

}  // namespace

absl::StatusOr<ValidationResult> TypeCheckerImpl::Check(
    std::unique_ptr<Ast> ast) const {
  auto& ast_impl = AstImpl::CastFromPublicAst(*ast);

  std::vector<TypeCheckIssue> issues;
  CEL_ASSIGN_OR_RETURN(auto generator,
                       NamespaceGenerator::Create(env_.container()));

  ResolveVisitor visitor(env_.container(), std::move(generator), env_, ast_impl,
                         issues);
  CEL_RETURN_IF_ERROR(visitor.status());

  TraversalOptions opts;
  opts.use_comprehension_callbacks = true;
  AstTraverse(ast_impl.root_expr(), visitor, opts);

  // Apply updates as needed.
  // Happens in a second pass to simplify validating that pointers haven't
  // been invalidated by other updates.

  for (const auto& issue : issues) {
    if (issue.severity() == Severity::kError) {
      return ValidationResult(std::move(issues));
    }
  }

  ResolveRewriter rewriter(visitor, ast_impl.reference_map());
  AstRewrite(ast_impl.root_expr(), rewriter);
  ast_impl.set_is_checked(true);

  return ValidationResult(std::move(ast), std::move(issues));
}

}  // namespace cel::checker_internal
