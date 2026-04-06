// Copyright 2026 Google LLC
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

#include "validator/comprehension_nesting_validator.h"

#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "common/expr.h"
#include "common/navigable_ast.h"
#include "validator/validator.h"

namespace cel {

namespace {

bool IsEmptyRangeComprehension(const NavigableAstNode& node) {
  ABSL_DCHECK(node.expr()->has_comprehension_expr());
  const auto& comp = node.expr()->comprehension_expr();
  return comp.has_iter_range() && comp.iter_range().has_list_expr() &&
         comp.iter_range().list_expr().elements().empty();
}

}  // namespace

Validation ComprehensionNestingLimitValidator(int limit) {
  return Validation(
      [limit](ValidationContext& context) -> bool {
        bool is_valid = true;
        for (const auto& node :
             context.navigable_ast().Root().DescendantsPostorder()) {
          if (node.node_kind() != NodeKind::kComprehension) {
            continue;
          }
          if (IsEmptyRangeComprehension(node)) {
            continue;
          }

          int count = 0;
          const NavigableAstNode* current = &node;
          while (current != nullptr) {
            if (current->node_kind() == NodeKind::kComprehension &&
                !IsEmptyRangeComprehension(*current)) {
              count++;
            }
            current = current->parent();
          }
          if (count > limit) {
            context.ReportErrorAt(
                node.expr()->id(),
                absl::StrCat("comprehension nesting level of ", count,
                             " exceeds limit of ", limit));
            is_valid = false;
            break;
          }
        }
        return is_valid;
      },
      "cel.validator.comprehension_nesting_limit");
}

}  // namespace cel
