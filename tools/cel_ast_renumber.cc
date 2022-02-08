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

#include "tools/cel_ast_renumber.h"

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/flat_hash_map.h"

namespace cel::ast {
namespace {

using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Expr;

// Renumbers expression IDs in a CheckedExpr.
// Note: does not renumber within macro_calls values.
class Renumberer {
 public:
  explicit Renumberer(int64_t next_id) : next_id_(next_id) {}

  // Returns the next free expression ID after renumbering.
  int64_t Renumber(CheckedExpr* cexpr) {
    old_to_new_.clear();
    Visit(cexpr->mutable_expr());
    CheckedExpr c2;  // scratch proto tables of the right type

    for (auto it = cexpr->type_map().begin(); it != cexpr->type_map().end();
         it++) {
      (*c2.mutable_type_map())[old_to_new_[it->first]] = it->second;
    }
    std::swap(*cexpr->mutable_type_map(), *c2.mutable_type_map());
    c2.mutable_type_map()->clear();

    for (auto it = cexpr->reference_map().begin();
         it != cexpr->reference_map().end(); it++) {
      (*c2.mutable_reference_map())[old_to_new_[it->first]] = it->second;
    }
    std::swap(*cexpr->mutable_reference_map(), *c2.mutable_reference_map());
    c2.mutable_reference_map()->clear();

    if (cexpr->has_source_info()) {
      auto* source_info = cexpr->mutable_source_info();
      auto* s2 = c2.mutable_source_info();

      for (auto it = source_info->positions().begin();
           it != source_info->positions().end(); it++) {
        (*s2->mutable_positions())[old_to_new_[it->first]] = it->second;
      }
      std::swap(*source_info->mutable_positions(), *s2->mutable_positions());
      s2->mutable_positions()->clear();

      for (auto it = source_info->macro_calls().begin();
           it != source_info->macro_calls().end(); it++) {
        (*s2->mutable_macro_calls())[old_to_new_[it->first]] = it->second;
      }
      std::swap(*source_info->mutable_macro_calls(),
                *s2->mutable_macro_calls());
      s2->mutable_macro_calls()->clear();
    }

    return next_id_;
  }

 private:
  // Insert mapping from old_id to the current next new_id.
  // Return next new_id.
  int64_t Renumber(int64_t old_id) {
    int64_t new_id = next_id_;
    ++next_id_;
    old_to_new_[old_id] = new_id;
    return new_id;
  }

  // Renumber this Expr and all sub-exprs and map entries.
  void Visit(Expr* e) {
    if (!e) {
      return;
    }
    switch (e->expr_kind_case()) {
      case Expr::kSelectExpr:
        Visit(e->mutable_select_expr()->mutable_operand());
        break;
      case Expr::kCallExpr: {
        auto call_expr = e->mutable_call_expr();
        if (call_expr->has_target()) {
          Visit(call_expr->mutable_target());
        }
        for (int i = 0; i < call_expr->args_size(); i++) {
          Visit(call_expr->mutable_args(i));
        }
      } break;
      case Expr::kListExpr: {
        auto list_expr = e->mutable_list_expr();
        for (int i = 0; i < list_expr->elements_size(); i++) {
          Visit(list_expr->mutable_elements(i));
        }
      } break;
      case Expr::kStructExpr: {
        auto struct_expr = e->mutable_struct_expr();
        for (int i = 0; i < struct_expr->entries_size(); i++) {
          auto entry = struct_expr->mutable_entries(i);
          if (entry->has_map_key()) {
            Visit(entry->mutable_map_key());
          }
          Visit(entry->mutable_value());
          entry->set_id(Renumber(entry->id()));
        }
      } break;
      case Expr::kComprehensionExpr: {
        auto comp_expr = e->mutable_comprehension_expr();
        Visit(comp_expr->mutable_iter_range());
        Visit(comp_expr->mutable_accu_init());
        Visit(comp_expr->mutable_loop_condition());
        Visit(comp_expr->mutable_loop_step());
        Visit(comp_expr->mutable_result());
      } break;
      default:
        // no other types have sub-expressions
        break;
    }
    e->set_id(Renumber(e->id()));  // do this last to mimic bottom-up build
  }

  int64_t next_id_;  // saved between Renumber() calls
  absl::flat_hash_map<int64_t, int64_t>
      old_to_new_;  // cleared between Renumber() calls
};

}  // namespace

// Renumbers expression IDs in a CheckedExpr in-place.
// This is intended to be used for injecting multiple sub-expressions into
// a merged expression.
// Note: does not renumber within macro_calls values.
// Returns the next free ID.
int64_t Renumber(int64_t starting_id, CheckedExpr* expr) {
  return Renumberer(starting_id).Renumber(expr);
}

}  // namespace cel::ast
