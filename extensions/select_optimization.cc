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

#include "extensions/select_optimization.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/types/struct_type.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/public/ast_rewrite_native.h"
#include "eval/public/source_position_native.h"
#include "eval/public/structs/legacy_type_info_apis.h"

namespace cel::extensions {
namespace {

using ::cel::ast_internal::AstImpl;
using ::cel::ast_internal::AstRewriterBase;
using ::cel::ast_internal::Constant;
using ::cel::ast_internal::ConstantKind;
using ::cel::ast_internal::Expr;
using ::cel::ast_internal::ExprKind;
using ::cel::ast_internal::SourcePosition;
using ::google::api::expr::runtime::PlannerContext;

// Represents a single select operation (field access or indexing).
// For struct-typed field accesses, includes the field name and the field
// number.
// TODO(uncreated-issue/51): initially, just covering the chained struct select case.
// Follow-ups will generalize for maps and lists.
struct SelectInstruction {
  int64_t number;
  absl::string_view name;
};

struct SelectPath {
  Expr* operand;
  std::vector<SelectInstruction> select_instructions;
  bool test_only;
  int64_t opt_index;
};

// Generates the AST representation of the qualification path for the optimized
// select branch. I.e., the list-typed second argument of the @cel.attribute
// call.
Expr MakeSelectPathExpr(
    const std::vector<SelectInstruction>& select_instructions) {
  Expr result;
  auto& list = result.mutable_list_expr().mutable_elements();
  list.reserve(select_instructions.size());
  for (const auto& instruction : select_instructions) {
    Expr ast_instruction;
    Expr field_number;
    field_number.mutable_const_expr().set_int64_value(instruction.number);
    Expr field_name;
    field_name.mutable_const_expr().set_string_value(
        std::string(instruction.name));
    auto& field_specifier =
        ast_instruction.mutable_list_expr().mutable_elements();
    field_specifier.push_back(std::move(field_number));
    field_specifier.push_back(std::move(field_name));

    list.push_back(std::move(ast_instruction));
  }
  return result;
}

// Returns a single select operation based on the inferred type of the operand
// and the field name. If the operand type doesn't define the field, returns
// nullopt.
absl::optional<SelectInstruction> GetSelectInstruction(
    const StructType& runtime_type, PlannerContext& planner_context,
    absl::string_view field_name) {
  auto field_or =
      runtime_type
          .FindFieldByName(planner_context.value_factory().type_manager(),
                           field_name)
          .value_or(absl::nullopt);
  if (field_or.has_value()) {
    return SelectInstruction{field_or->number, field_name};
  }
  return absl::nullopt;
}

class RewriterImpl : public AstRewriterBase {
 public:
  RewriterImpl(const AstImpl& ast, PlannerContext& planner_context)
      : ast_(ast),
        planner_context_(planner_context),
        type_factory_(MemoryManager::Global()) {}

  void PreVisitExpr(const Expr* expr, const SourcePosition* position) override {
    path_.push_back(expr);
    if (expr->has_select_expr()) {
      const Expr& operand = expr->select_expr().operand();
      const std::string& field_name = expr->select_expr().field();
      // Select optimization can generalize to lists and maps, but for now only
      // support message traversal.
      const ast_internal::Type& checker_type = ast_.GetType(operand.id());
      if (checker_type.has_message_type()) {
        auto type_or = GetRuntimeType(checker_type.message_type().type());
        if (type_or.has_value() && (*type_or)->Is<StructType>()) {
          const StructType& runtime_type = (*type_or)->As<StructType>();
          auto field_or =
              GetSelectInstruction(runtime_type, planner_context_, field_name);

          if (field_or.has_value()) {
            candidates_[expr] = std::move(field_or).value();
          }
        }
      }
    }
  }

  bool PostVisitRewrite(Expr* expr, const SourcePosition* position) override {
    path_.pop_back();
    auto candidate_iter = candidates_.find(expr);
    if (candidate_iter == candidates_.end()) {
      return false;
    }
    if (!path_.empty() && candidates_.find(path_.back()) != candidates_.end()) {
      // parent is optimizeable, defer rewriting until we consider the parent.
      return false;
    }

    SelectPath path = GetSelectPath(expr);

    // generate the new cel.attribute call.
    absl::string_view fn = path.test_only ? kFieldsHas : kCelAttribute;

    Expr operand(std::move(*path.operand));
    Expr call;
    call.set_id(expr->id());
    call.mutable_call_expr().set_function(std::string(fn));
    call.mutable_call_expr().mutable_args().reserve(
        2 + (path.opt_index >= 0 ? 1 : 0));
    call.mutable_call_expr().mutable_args().push_back(std::move(operand));
    call.mutable_call_expr().mutable_args().push_back(
        MakeSelectPathExpr(path.select_instructions));
    if (path.opt_index >= 0) {
      call.mutable_call_expr().mutable_args().push_back(
          Expr(-1, ExprKind(Constant(ConstantKind(path.opt_index)))));
    }

    *expr = std::move(call);

    return true;
  }

 private:
  SelectPath GetSelectPath(Expr* expr) {
    SelectPath result;
    // TODO(uncreated-issue/52): update after C++ runtime ast supports optionals.
    result.opt_index = -1;
    result.test_only = false;
    Expr* operand = expr;
    auto candidate_iter = candidates_.find(operand);
    while (candidate_iter != candidates_.end()) {
      if (operand->select_expr().test_only()) {
        result.test_only = true;
      }
      result.select_instructions.push_back(candidate_iter->second);
      operand = &(operand->mutable_select_expr().mutable_operand());
      candidate_iter = candidates_.find(operand);
    }
    absl::c_reverse(result.select_instructions);
    result.operand = operand;
    return result;
  }

  absl::optional<Handle<Type>> GetRuntimeType(absl::string_view type_name) {
    return planner_context_.value_factory()
        .type_manager()
        .ResolveType(type_name)
        .value_or(absl::nullopt);
  }

  const AstImpl& ast_;
  PlannerContext& planner_context_;
  TypeFactory type_factory_;
  // ids of potentially optimizeable expr nodes.
  absl::flat_hash_map<const Expr*, SelectInstruction> candidates_;
  std::vector<const Expr*> path_;
};

}  // namespace

absl::Status SelectOptimizationAstUpdater::UpdateAst(PlannerContext& context,
                                                     AstImpl& ast) const {
  RewriterImpl rewriter(ast, context);

  ast_internal::AstRewrite(&ast.root_expr(), &ast.source_info(), &rewriter);
  return absl::OkStatus();
}

}  // namespace cel::extensions
