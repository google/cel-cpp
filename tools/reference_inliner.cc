// Copyright 2021 Google LLC
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

#include "tools/reference_inliner.h"

#include <deque>
#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "eval/public/ast_rewrite.h"
#include "eval/public/ast_traverse.h"
#include "eval/public/ast_visitor_base.h"
#include "eval/public/source_position.h"
#include "tools/cel_ast_renumber.h"
#include "re2/re2.h"
#include "re2/regexp.h"

namespace cel::ast {
namespace {

using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::runtime::AstRewrite;
using ::google::api::expr::runtime::AstRewriterBase;
using ::google::api::expr::runtime::AstTraverse;
using ::google::api::expr::runtime::AstVisitorBase;
using ::google::api::expr::runtime::SourcePosition;

// Filter for legal select paths.
static LazyRE2 kIdentRegex = {
    R"(([_a-zA-Z][_a-zA-Z0-9]*)(\.[_a-zA-Z][_a-zA-Z0-9]*)*)"};

using IdentExpr = google::api::expr::v1alpha1::Expr::Ident;
using RewriteRuleMap =
    absl::flat_hash_map<absl::string_view, const CheckedExpr*>;

void MergeMetadata(const CheckedExpr& to_insert, CheckedExpr* base) {
  base->mutable_reference_map()->insert(to_insert.reference_map().begin(),
                                        to_insert.reference_map().end());
  base->mutable_type_map()->insert(to_insert.type_map().begin(),
                                   to_insert.type_map().end());
  auto* source_info = base->mutable_source_info();
  source_info->mutable_positions()->insert(
      to_insert.source_info().positions().begin(),
      to_insert.source_info().positions().end());

  source_info->mutable_macro_calls()->insert(
      to_insert.source_info().macro_calls().begin(),
      to_insert.source_info().macro_calls().end());
}

void PruneMetadata(const std::vector<int64_t>& ids, CheckedExpr* base) {
  auto* source_info = base->mutable_source_info();
  for (int64_t i : ids) {
    base->mutable_reference_map()->erase(i);
    base->mutable_type_map()->erase(i);
    source_info->mutable_positions()->erase(i);
    source_info->mutable_macro_calls()->erase(i);
  }
}

class InlinerRewrite : public AstRewriterBase {
 public:
  InlinerRewrite(const RewriteRuleMap& rewrite_rules, CheckedExpr* base,
                 int64_t next_id)
      : base_(base), rewrite_rules_(rewrite_rules), next_id_(next_id) {}
  void PostVisitIdent(const IdentExpr* ident, const Expr* expr,
                      const SourcePosition* source_pos) override {
    // e.g. `com.google.Identifier` would have a path of
    // SelectExpr("Identifier"), SelectExpr("google"), IdentExpr("com")
    std::vector<absl::string_view> qualifiers{ident->name()};
    for (int i = path_.size() - 2; i >= 0; i--) {
      if (!path_[i]->has_select_expr() || path_[i]->select_expr().test_only()) {
        break;
      }
      qualifiers.push_back(path_[i]->select_expr().field());
    }

    // Check longest possible match first then less specific qualifiers.
    for (int path_len = qualifiers.size(); path_len >= 1; path_len--) {
      int path_len_offset = qualifiers.size() - path_len;
      std::string candidate = absl::StrJoin(
          qualifiers.begin(), qualifiers.end() - path_len_offset, ".");
      auto rule_it = rewrite_rules_.find(candidate);
      if (rule_it != rewrite_rules_.end()) {
        std::vector<int64_t> invalidated_ids;
        invalidated_ids.reserve(path_len);
        for (int offset = 0; offset < path_len; offset++) {
          invalidated_ids.push_back(path_[path_.size() - (1 + offset)]->id());
        }

        // The target the root node of the reference subtree to get updated.
        int64_t root_id = path_[path_.size() - path_len]->id();
        rewrite_positions_[root_id] =
            Rewrite{std::move(invalidated_ids), rule_it->second};
        // Any other rewrites are redundant.
        break;
      }
    }
  }

  bool PostVisitRewrite(Expr* expr, const SourcePosition* source_pos) override {
    auto it = rewrite_positions_.find(expr->id());
    if (it == rewrite_positions_.end()) {
      return false;
    }
    const Rewrite& rewrite = (it->second);
    CheckedExpr new_sub_expr = *rewrite.rewrite;
    next_id_ = Renumber(next_id_, &new_sub_expr);
    MergeMetadata(new_sub_expr, base_);
    expr->Swap(new_sub_expr.mutable_expr());
    PruneMetadata(rewrite.invalidated_ids, base_);
    return true;
  }

  void TraversalStackUpdate(absl::Span<const Expr*> path) override {
    path_ = path;
  }

 private:
  struct Rewrite {
    std::vector<int64_t> invalidated_ids;
    const CheckedExpr* rewrite;
  };
  absl::Span<const Expr*> path_;
  absl::flat_hash_map<int64_t, Rewrite> rewrite_positions_;
  CheckedExpr* base_;
  const RewriteRuleMap& rewrite_rules_;
  int next_id_;
};

// Validate visitor is used to check that an AST is safe for the inlining
// utility -- hand-rolled ASTs may not have a legal numbering for the nodes in
// the tree and metadata maps (i.e. a unique id for each node).
// CheckedExprs generated from a type checker should always be safe.
class ValidateVisitor : public AstVisitorBase {
 public:
  ValidateVisitor() : max_id_(0), is_valid_(true) {}
  void PostVisitExpr(const Expr* expr, const SourcePosition* pos) override {
    auto [it, inserted] = visited_.insert(expr->id());
    if (!inserted) {
      is_valid_ = false;
    }
    if (expr->id() > max_id_) {
      max_id_ = expr->id();
    }
  }
  bool IdsValid() { return is_valid_; }
  int64_t GetMaxId() { return max_id_; }

 private:
  int64_t max_id_;
  absl::flat_hash_set<int64_t> visited_;
  bool is_valid_;
};

}  // namespace

absl::Status Inliner::SetRewriteRule(absl::string_view qualified_identifier,
                                     const CheckedExpr& expr) {
  if (!RE2::FullMatch(re2::StringPiece(qualified_identifier.data(), qualified_identifier.size()), *kIdentRegex)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported identifier for CheckedExpr rewrite rule: ",
                     qualified_identifier));
  }
  rewrites_.insert_or_assign(qualified_identifier, &expr);
  return absl::OkStatus();
}

absl::StatusOr<CheckedExpr> Inliner::Inline(const CheckedExpr& expr) const {
  // Determine if the source expr has a legal numbering and pick out the next
  // available id.
  ValidateVisitor validator;
  AstTraverse(&expr.expr(), &expr.source_info(), &validator);
  if (!validator.IdsValid()) {
    return absl::InvalidArgumentError("Invalid Expr IDs");
  }
  CheckedExpr output = expr;
  InlinerRewrite rewrite_visitor(rewrites_, &output, validator.GetMaxId() + 1);
  AstRewrite(output.mutable_expr(), &output.source_info(), &rewrite_visitor);
  return output;
}

}  // namespace cel::ast
