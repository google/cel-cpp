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

#include "testutil/test_macros.h"

#include <cstdint>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/expr.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"
#include "parser/macro_registry.h"

namespace cel::test {

namespace {

bool IsCelNamespace(const Expr& target) {
  return target.has_ident_expr() && target.ident_expr().name() == "cel";
}

absl::optional<Expr> CelBlockMacroExpander(MacroExprFactory& factory,
                                           Expr& target,
                                           absl::Span<Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  Expr& bindings_arg = args[0];
  if (!bindings_arg.has_list_expr()) {
    return factory.ReportErrorAt(
        bindings_arg, "cel.block requires the first arg to be a list literal");
  }
  return factory.NewCall("cel.@block", args);
}

absl::optional<Expr> CelIndexMacroExpander(MacroExprFactory& factory,
                                           Expr& target,
                                           absl::Span<Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  Expr& index_arg = args[0];
  if (!index_arg.has_const_expr() || !index_arg.const_expr().has_int_value()) {
    return factory.ReportErrorAt(
        index_arg, "cel.index requires a single non-negative int constant arg");
  }
  int64_t index = index_arg.const_expr().int_value();
  if (index < 0) {
    return factory.ReportErrorAt(
        index_arg, "cel.index requires a single non-negative int constant arg");
  }
  return factory.NewIdent(absl::StrCat("@index", index));
}

absl::optional<Expr> CelIterVarMacroExpander(MacroExprFactory& factory,
                                             Expr& target,
                                             absl::Span<Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  Expr& depth_arg = args[0];
  if (!depth_arg.has_const_expr() || !depth_arg.const_expr().has_int_value() ||
      depth_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        depth_arg, "cel.iterVar requires two non-negative int constant args");
  }
  Expr& unique_arg = args[1];
  if (!unique_arg.has_const_expr() ||
      !unique_arg.const_expr().has_int_value() ||
      unique_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        unique_arg, "cel.iterVar requires two non-negative int constant args");
  }
  return factory.NewIdent(
      absl::StrCat("@it:", depth_arg.const_expr().int_value(), ":",
                   unique_arg.const_expr().int_value()));
}

absl::optional<Expr> CelAccuVarMacroExpander(MacroExprFactory& factory,
                                             Expr& target,
                                             absl::Span<Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  Expr& depth_arg = args[0];
  if (!depth_arg.has_const_expr() || !depth_arg.const_expr().has_int_value() ||
      depth_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        depth_arg, "cel.accuVar requires two non-negative int constant args");
  }
  Expr& unique_arg = args[1];
  if (!unique_arg.has_const_expr() ||
      !unique_arg.const_expr().has_int_value() ||
      unique_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        unique_arg, "cel.accuVar requires two non-negative int constant args");
  }
  return factory.NewIdent(
      absl::StrCat("@ac:", depth_arg.const_expr().int_value(), ":",
                   unique_arg.const_expr().int_value()));
}

Macro MakeCelBlockMacro() {
  auto macro_or_status = Macro::Receiver("block", 2, CelBlockMacroExpander);
  ABSL_CHECK_OK(macro_or_status);  // Crash OK
  return std::move(*macro_or_status);
}

Macro MakeCelIndexMacro() {
  auto macro_or_status = Macro::Receiver("index", 1, CelIndexMacroExpander);
  ABSL_CHECK_OK(macro_or_status);  // Crash OK
  return std::move(*macro_or_status);
}

Macro MakeCelIterVarMacro() {
  auto macro_or_status = Macro::Receiver("iterVar", 2, CelIterVarMacroExpander);
  ABSL_CHECK_OK(macro_or_status);  // Crash OK
  return std::move(*macro_or_status);
}

Macro MakeCelAccuVarMacro() {
  auto macro_or_status = Macro::Receiver("accuVar", 2, CelAccuVarMacroExpander);
  ABSL_CHECK_OK(macro_or_status);  // Crash OK
  return std::move(*macro_or_status);
}

}  // namespace

const Macro& CelBlockMacro() {
  static const absl::NoDestructor<Macro> macro(MakeCelBlockMacro());
  return *macro;
}

const Macro& CelIndexMacro() {
  static const absl::NoDestructor<Macro> macro(MakeCelIndexMacro());
  return *macro;
}

const Macro& CelIterVarMacro() {
  static const absl::NoDestructor<Macro> macro(MakeCelIterVarMacro());
  return *macro;
}

const Macro& CelAccuVarMacro() {
  static const absl::NoDestructor<Macro> macro(MakeCelAccuVarMacro());
  return *macro;
}

absl::Status RegisterTestMacros(MacroRegistry& registry) {
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(CelBlockMacro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(CelIndexMacro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(CelIterVarMacro()));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(CelAccuVarMacro()));
  return absl::OkStatus();
}

}  // namespace cel::test
