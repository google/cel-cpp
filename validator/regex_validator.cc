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

#include "validator/regex_validator.h"

#include <utility>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/constant.h"
#include "common/expr.h"
#include "common/navigable_ast.h"
#include "internal/re2_options.h"
#include "validator/validator.h"
#include "re2/re2.h"

namespace cel {

namespace {

bool CheckPattern(ValidationContext& context, const NavigableAstNode& node,
                  int arg_index) {
  ABSL_DCHECK(node.expr()->has_call_expr());
  const auto& call_expr = node.expr()->call_expr();

  const Expr* pattern_expr = nullptr;

  if (call_expr.has_target()) {
    if (arg_index == 0) {
      pattern_expr = &call_expr.target();
    } else if (call_expr.args().size() > arg_index - 1) {
      pattern_expr = &call_expr.args()[arg_index - 1];
    }
  } else if (call_expr.args().size() > arg_index) {
    pattern_expr = &call_expr.args()[arg_index];
  }

  if (pattern_expr == nullptr || !pattern_expr->has_const_expr()) {
    return true;
  }

  const auto& const_expr = pattern_expr->const_expr();
  if (!const_expr.has_string_value()) {
    return true;
  }

  absl::string_view pattern_string = const_expr.string_value();
  RE2 re(pattern_string, internal::MakeRE2Options());
  if (!re.ok()) {
    context.ReportErrorAt(
        pattern_expr->id(),
        absl::StrCat("invalid regular expression: ", re.error()));
    return false;
  }
  return true;
}

}  // namespace

Validation RegexPatternValidator(
    absl::string_view id, std::vector<RegexPatternValidatorConfig> config) {
  return Validation(
      [config = std::move(config)](ValidationContext& context) -> bool {
        bool result = true;
        for (const auto& node :
             context.navigable_ast().Root().DescendantsPostorder()) {
          if (node.node_kind() == NodeKind::kCall) {
            for (const auto& config : config) {
              if (node.expr()->call_expr().function() == config.function_name) {
                if (!CheckPattern(context, node, config.pattern_arg_index)) {
                  result = false;
                }
                break;
              }
            }
          }
        }
        return result;
      },
      id);
}

}  // namespace cel
