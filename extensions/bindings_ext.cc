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

#include "extensions/bindings_ext.h"

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "parser/macro.h"
#include "parser/source_factory.h"

namespace cel::extensions {

namespace {
using google::api::expr::v1alpha1::Expr;
using google::api::expr::parser::SourceFactory;

static constexpr char kCelNamespace[] = "cel";
static constexpr char kBind[] = "bind";
static constexpr char kUnusedIterVar[] = "#unused";

bool isTargetNamespace(const Expr& target) {
  switch (target.expr_kind_case()) {
    case Expr::kIdentExpr:
      return target.ident_expr().name() == kCelNamespace;
    default:
      return false;
  }
}

}  // namespace

std::vector<Macro> bindings_macros() {
  absl::StatusOr<Macro> cel_bind = Macro::Receiver(
      kBind, 3,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        if (!isTargetNamespace(target)) {
          return Expr();
        }
        const Expr& var_ident = args[0];
        if (!var_ident.has_ident_expr()) {
          return sf->ReportError(
              var_ident.id(),
              "cel.bind() variable name must be a simple identifier");
        }
        const std::string& var_name = var_ident.ident_expr().name();
        const Expr& var_init = args[1];
        const Expr& result_expr = args[2];
        return sf->NewComprehension(
            macro_id, kUnusedIterVar,
            sf->NewList(sf->NextMacroId(macro_id), std::vector<Expr>()),
            var_name, var_init,
            sf->NewLiteralBoolForMacro(sf->NextMacroId(macro_id), false),
            sf->NewIdentForMacro(sf->NextMacroId(macro_id), var_name),
            result_expr);
      });
  return {*cel_bind};
}

}  // namespace cel::extensions
