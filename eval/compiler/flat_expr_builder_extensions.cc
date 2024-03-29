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
#include "eval/compiler/flat_expr_builder_extensions.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/variant.h"
#include "base/ast_internal/expr.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

using Subexpression = google::api::expr::runtime::ProgramBuilder::Subexpression;

Subexpression::Subexpression(const cel::ast_internal::Expr* self,
                             ProgramBuilder* owner)
    : self_(self), parent_(nullptr), subprogram_map_(owner->subprogram_map_) {}

size_t Subexpression::ComputeSize() const {
  if (IsFlattened()) {
    return flattened_elements().size();
  }
  std::vector<const Subexpression*> to_expand{this};
  size_t size = 0;
  while (!to_expand.empty()) {
    const auto* expr = to_expand.back();
    to_expand.pop_back();
    if (expr->IsFlattened()) {
      size += expr->flattened_elements().size();
      continue;
    }
    for (const auto& elem : expr->elements()) {
      if (auto* child = absl::get_if<std::unique_ptr<Subexpression>>(&elem);
          child != nullptr) {
        to_expand.push_back(child->get());
      } else {
        size += 1;
      }
    }
  }
  return size;
}

Subexpression::~Subexpression() {
  auto map_ptr = subprogram_map_.lock();
  if (map_ptr == nullptr) {
    return;
  }
  auto it = map_ptr->find(self_);
  if (it != map_ptr->end() && it->second == this) {
    map_ptr->erase(it);
  }
}

std::unique_ptr<Subexpression> Subexpression::ExtractChild(
    Subexpression* child) {
  if (IsFlattened()) {
    return nullptr;
  }
  for (auto iter = elements().begin(); iter != elements().end(); ++iter) {
    Subexpression::Element& element = *iter;
    if (!absl::holds_alternative<std::unique_ptr<Subexpression>>(element)) {
      continue;
    }
    auto& subexpression_owner =
        absl::get<std::unique_ptr<Subexpression>>(element);
    if (subexpression_owner.get() != child) {
      continue;
    }
    std::unique_ptr<Subexpression> result = std::move(subexpression_owner);
    elements().erase(iter);
    return result;
  }
  return nullptr;
}

int Subexpression::CalculateOffset(int base, int target) const {
  ABSL_DCHECK(!IsFlattened());
  ABSL_DCHECK_GE(base, 0);
  ABSL_DCHECK_GE(target, 0);
  ABSL_DCHECK_LE(base, elements().size());
  ABSL_DCHECK_LE(target, elements().size());

  int sign = 1;

  if (target <= base) {
    // target is before base so have to consider the size of the base step and
    // target (offset is end of base to beginning of target).
    int tmp = base;
    base = target - 1;
    target = tmp + 1;
    sign = -1;
  }

  int sum = 0;
  for (int i = base + 1; i < target; ++i) {
    const auto& element = elements()[i];
    if (auto* subexpr = absl::get_if<std::unique_ptr<Subexpression>>(&element);
        subexpr != nullptr) {
      sum += (*subexpr)->ComputeSize();
    } else {
      sum += 1;
    }
  }

  return sign * sum;
}

void Subexpression::Flatten() {
  struct Record {
    Subexpression* subexpr;
    size_t offset;
  };

  if (IsFlattened()) {
    return;
  }

  std::vector<std::unique_ptr<const ExpressionStep>> flat;

  std::vector<Record> flatten_stack;

  flatten_stack.push_back({this, 0});
  while (!flatten_stack.empty()) {
    Record top = flatten_stack.back();
    flatten_stack.pop_back();
    size_t offset = top.offset;
    auto& subexpr = top.subexpr;
    if (subexpr->IsFlattened()) {
      absl::c_move(subexpr->flattened_elements(), std::back_inserter(flat));
      continue;
    }
    size_t size = subexpr->elements().size();
    size_t i = offset;
    for (; i < size; ++i) {
      auto& element = subexpr->elements()[i];
      if (auto* child = absl::get_if<std::unique_ptr<Subexpression>>(&element);
          child != nullptr) {
        flatten_stack.push_back({subexpr, i + 1});
        flatten_stack.push_back({child->get(), 0});
        break;
      } else {
        flat.push_back(
            absl::get<std::unique_ptr<ExpressionStep>>(std::move(element)));
      }
    }
    if (i >= size && subexpr != this) {
      // delete incrementally instead of all at once.
      subexpr->program_.emplace<std::vector<Subexpression::Element>>();
    }
  }
  program_ = std::move(flat);
}

bool Subexpression::ExtractTo(
    std::vector<std::unique_ptr<const ExpressionStep>>& out) {
  if (!IsFlattened()) {
    return false;
  }

  out.reserve(out.size() + flattened_elements().size());
  absl::c_move(flattened_elements(), std::back_inserter(out));
  program_.emplace<std::vector<Element>>();

  return true;
}

std::vector<std::unique_ptr<const ExpressionStep>>
ProgramBuilder::FlattenSubexpression(std::unique_ptr<Subexpression> expr) {
  std::vector<std::unique_ptr<const ExpressionStep>> out;

  if (!expr) {
    return out;
  }

  expr->Flatten();
  expr->ExtractTo(out);
  return out;
}

ProgramBuilder::ProgramBuilder()
    : root_(nullptr),
      current_(nullptr),
      subprogram_map_(std::make_shared<SubprogramMap>()) {}

ExecutionPath ProgramBuilder::FlattenMain() {
  auto out = FlattenSubexpression(std::move(root_));
  return out;
}

std::vector<ExecutionPath> ProgramBuilder::FlattenSubexpressions() {
  std::vector<ExecutionPath> out;
  out.reserve(extracted_subexpressions_.size());
  for (auto& subexpression : extracted_subexpressions_) {
    out.push_back(FlattenSubexpression(std::move(subexpression)));
  }
  extracted_subexpressions_.clear();
  return out;
}

absl::Nullable<Subexpression*> ProgramBuilder::EnterSubexpression(
    const cel::ast_internal::Expr* expr) {
  std::unique_ptr<Subexpression> subexpr = MakeSubexpression(expr);
  auto* result = subexpr.get();
  if (current_ == nullptr) {
    root_ = std::move(subexpr);
    current_ = result;
    return result;
  }

  current_->AddSubexpression(std::move(subexpr));
  result->parent_ = current_->self_;
  current_ = result;
  return result;
}

absl::Nullable<Subexpression*> ProgramBuilder::ExitSubexpression(
    const cel::ast_internal::Expr* expr) {
  ABSL_DCHECK(expr == current_->self_);
  ABSL_DCHECK(GetSubexpression(expr) == current_);

  Subexpression* result = GetSubexpression(current_->parent_);
  ABSL_DCHECK(result != nullptr || current_ == root_.get());
  current_ = result;
  return result;
}

absl::Nullable<Subexpression*> ProgramBuilder::GetSubexpression(
    const cel::ast_internal::Expr* expr) {
  auto it = subprogram_map_->find(expr);
  if (it == subprogram_map_->end()) {
    return nullptr;
  }

  return it->second;
}

void ProgramBuilder::AddStep(std::unique_ptr<ExpressionStep> step) {
  if (current_ == nullptr) {
    return;
  }
  current_->AddStep(std::move(step));
}

int ProgramBuilder::ExtractSubexpression(const cel::ast_internal::Expr* expr) {
  auto it = subprogram_map_->find(expr);
  if (it == subprogram_map_->end()) {
    return -1;
  }
  auto* subexpression = it->second;
  auto parent_it = subprogram_map_->find(subexpression->parent_);
  if (parent_it == subprogram_map_->end()) {
    return -1;
  }

  auto* parent = parent_it->second;

  std::unique_ptr<Subexpression> subexpression_owner =
      parent->ExtractChild(subexpression);

  if (subexpression_owner == nullptr) {
    return -1;
  }

  extracted_subexpressions_.push_back(std::move(subexpression_owner));
  return extracted_subexpressions_.size() - 1;
}

std::unique_ptr<Subexpression> ProgramBuilder::MakeSubexpression(
    const cel::ast_internal::Expr* expr) {
  auto* subexpr = new Subexpression(expr, this);
  (*subprogram_map_)[expr] = subexpr;
  return absl::WrapUnique(subexpr);
}

bool PlannerContext::IsSubplanInspectable(
    const cel::ast_internal::Expr& node) const {
  return program_builder_.GetSubexpression(&node) != nullptr;
}

ExecutionPathView PlannerContext::GetSubplan(
    const cel::ast_internal::Expr& node) {
  auto* subexpression = program_builder_.GetSubexpression(&node);
  if (subexpression == nullptr) {
    return ExecutionPathView();
  }
  subexpression->Flatten();
  return subexpression->flattened_elements();
}

absl::StatusOr<ExecutionPath> PlannerContext::ExtractSubplan(
    const cel::ast_internal::Expr& node) {
  auto* subexpression = program_builder_.GetSubexpression(&node);
  if (subexpression == nullptr) {
    return absl::InternalError(
        "attempted to update program step for untracked expr node");
  }

  subexpression->Flatten();

  ExecutionPath out;
  subexpression->ExtractTo(out);

  return out;
}

absl::Status PlannerContext::ReplaceSubplan(const cel::ast_internal::Expr& node,
                                            ExecutionPath path) {
  auto* subexpression = program_builder_.GetSubexpression(&node);
  if (subexpression == nullptr) {
    return absl::InternalError(
        "attempted to update program step for untracked expr node");
  }

  // Make sure structure for descendents is erased.
  if (!subexpression->IsFlattened()) {
    subexpression->Flatten();
  }

  subexpression->flattened_elements() = std::move(path);

  return absl::OkStatus();
}

absl::Status PlannerContext::AddSubplanStep(
    const cel::ast_internal::Expr& node, std::unique_ptr<ExpressionStep> step) {
  auto* subexpression = program_builder_.GetSubexpression(&node);

  if (subexpression == nullptr) {
    return absl::InternalError(
        "attempted to update program step for untracked expr node");
  }

  subexpression->AddStep(std::move(step));

  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
