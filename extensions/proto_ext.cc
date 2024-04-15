// Copyright 2022 Google LLC
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

#include "extensions/proto_ext.h"

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "parser/macro.h"
#include "parser/source_factory.h"

namespace cel::extensions {

namespace {
using google::api::expr::v1alpha1::Expr;
using google::api::expr::parser::SourceFactory;

static constexpr char kProtoNamespace[] = "proto";
static constexpr char kGetExt[] = "getExt";
static constexpr char kHasExt[] = "hasExt";

absl::optional<std::string> validateExtensionIdentifier(const Expr& expr) {
  switch (expr.expr_kind_case()) {
    case Expr::kSelectExpr: {
      if (expr.select_expr().test_only()) {
        return absl::nullopt;
      }
      auto op_name = validateExtensionIdentifier(expr.select_expr().operand());
      if (!op_name.has_value()) {
        return absl::nullopt;
      }
      return absl::optional<std::string>(
          absl::StrCat(*op_name, ".", expr.select_expr().field()));
    }
    case Expr::kIdentExpr:
      return absl::optional<std::string>(expr.ident_expr().name());
    default:
      return absl::nullopt;
  }
}

absl::optional<std::string> getExtensionFieldName(const Expr& expr) {
  switch (expr.expr_kind_case()) {
    case Expr::kSelectExpr:
      return validateExtensionIdentifier(expr);
    default:
      return absl::nullopt;
  }
}

bool isExtensionCall(const Expr& target) {
  switch (target.expr_kind_case()) {
    case Expr::kIdentExpr:
      return target.ident_expr().name() == kProtoNamespace;
    default:
      return false;
  }
}

}  // namespace

std::vector<Macro> proto_macros() {
  absl::StatusOr<Macro> getExt = Macro::Receiver(
      kGetExt, 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        if (!isExtensionCall(target)) {
          return Expr();
        }
        auto extFieldName = getExtensionFieldName(args[1]);
        if (!extFieldName.has_value()) {
          return sf->ReportError(args[1].id(), "invalid extension field");
        }
        return sf->NewSelectForMacro(macro_id, args[0], *extFieldName);
      });
  absl::StatusOr<Macro> hasExt = Macro::Receiver(
      kHasExt, 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        if (!isExtensionCall(target)) {
          return Expr();
        }
        auto extFieldName = getExtensionFieldName(args[1]);
        if (!extFieldName.has_value()) {
          return sf->ReportError(args[1].id(), "invalid extension field");
        }
        return sf->NewPresenceTestForMacro(macro_id, args[0], *extFieldName);
      });
  return {*hasExt, *getExt};
}

}  // namespace cel::extensions
