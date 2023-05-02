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

#include <utility>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "base/ast_internal.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

ExecutionPathView PlannerContext::GetSubplan(
    const cel::ast::internal::Expr& node) const {
  auto iter = program_tree_.find(&node);
  if (iter == program_tree_.end()) {
    return {};
  }

  const ProgramInfo& info = iter->second;

  if (info.range_len == -1) {
    // Initial planning for this node hasn't finished.
    return {};
  }

  return absl::MakeConstSpan(execution_path_)
      .subspan(info.range_start, info.range_len);
}

absl::Status PlannerContext::ReplaceSubplan(
    const cel::ast::internal::Expr& node, ExecutionPath path) {
  auto iter = program_tree_.find(&node);
  if (iter == program_tree_.end()) {
    return absl::InternalError("attempted to rewrite unknown program step");
  }

  ProgramInfo& info = iter->second;

  if (info.range_len == -1) {
    // Initial planning for this node hasn't finished.
    return absl::InternalError(
        "attempted to rewrite program step before completion.");
  }

  int new_len = path.size();
  int old_len = info.range_len;
  int delta = new_len - old_len;

  // If the replacement is differently sized, insert or erase program step
  // slots at the replacement point before applying the replacement steps.
  if (delta > 0) {
    // Insert enough spaces to accommodate the replacement plan.
    for (int i = 0; i < delta; ++i) {
      execution_path_.insert(
          execution_path_.begin() + info.range_start + info.range_len, nullptr);
    }
  } else if (delta < 0) {
    // Erase spaces down to the size of the new sub plan.
    execution_path_.erase(execution_path_.begin() + info.range_start,
                          execution_path_.begin() + info.range_start - delta);
  }

  absl::c_move(std::move(path), execution_path_.begin() + info.range_start);

  info.range_len = new_len;

  // Adjust program range for parent and sibling expr nodes if we needed to
  // realign them for the replacement. Note: the program structure is only
  // maintained for the immediate neighborhood of node being processed by the
  // planner, so descendants are not recursively updated.
  auto parent_iter = program_tree_.find(info.parent);
  if (parent_iter != program_tree_.end() && delta != 0) {
    ProgramInfo& parent_info = parent_iter->second;
    if (parent_info.range_len != -1) {
      parent_info.range_len += delta;
    }

    int idx = -1;
    for (int i = 0; i < parent_info.children.size(); ++i) {
      if (parent_info.children[i] == &node) {
        idx = i;
        break;
      }
    }
    if (idx > -1) {
      for (int j = idx + 1; j < parent_info.children.size(); ++j) {
        program_tree_[parent_info.children[j]].range_start += delta;
      }
    }
  }

  // Invalidate any program tree information for dependencies of the rewritten
  // node.
  for (const cel::ast::internal::Expr* e : info.children) {
    program_tree_.erase(e);
  }

  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
