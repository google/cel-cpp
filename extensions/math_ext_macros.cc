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

#include "extensions/math_ext_macros.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "parser/macro.h"
#include "parser/source_factory.h"

namespace cel::extensions {

namespace {

static constexpr absl::string_view kMathNamespace = "math";
static constexpr absl::string_view kLeast = "least";
static constexpr absl::string_view kGreatest = "greatest";

static constexpr char kMathMin[] = "math.@min";
static constexpr char kMathMax[] = "math.@max";

using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::parser::SourceFactory;

bool isTargetNamespace(const Expr &target) {
  switch (target.expr_kind_case()) {
    case Expr::kIdentExpr:
      return target.ident_expr().name() == kMathNamespace;
    default:
      return false;
  }
}

bool isValidArgType(const Expr &arg) {
  switch (arg.expr_kind_case()) {
    case google::api::expr::v1alpha1::Expr::kConstExpr:
      if (arg.const_expr().has_double_value() ||
          arg.const_expr().has_int64_value() ||
          arg.const_expr().has_uint64_value()) {
        return true;
      }
      return false;
    case google::api::expr::v1alpha1::Expr::kListExpr:
    case google::api::expr::v1alpha1::Expr::kStructExpr:  // fall through
      return false;
    default:
      return true;
  }
}

absl::optional<Expr> checkInvalidArgs(const std::shared_ptr<SourceFactory> &sf,
                                      const absl::string_view macro,
                                      const std::vector<Expr> &args) {
  for (const auto &arg : args) {
    if (!isValidArgType(arg)) {
      return absl::optional<Expr>(sf->ReportError(
          arg.id(),
          absl::StrCat(macro, " simple literal arguments must be numeric")));
    }
  }

  return absl::nullopt;
}

bool isListLiteralWithValidArgs(const Expr &arg) {
  switch (arg.expr_kind_case()) {
    case google::api::expr::v1alpha1::Expr::kListExpr: {
      const auto &list_expr = arg.list_expr();
      if (list_expr.elements().empty()) {
        return false;
      }

      for (const auto &elem : list_expr.elements()) {
        if (!isValidArgType(elem)) {
          return false;
        }
      }
      return true;
    }
    default: {
      return false;
    }
  }
}

}  // namespace

std::vector<Macro> math_macros() {
  absl::StatusOr<Macro> least = Macro::ReceiverVarArg(
      kLeast, [](const std::shared_ptr<SourceFactory> &sf, int64_t macro_id,
                 const Expr &target, const std::vector<Expr> &args) {
        if (!isTargetNamespace(target)) {
          return Expr();
        }

        switch (args.size()) {
          case 0:
            return sf->ReportError(
                target.id(), "math.least() requires at least one argument.");
          case 1: {
            if (!isListLiteralWithValidArgs(args[0]) &&
                !isValidArgType(args[0])) {
              return sf->ReportError(
                  args[0].id(), "math.least() invalid single argument value.");
            }

            return sf->NewGlobalCallForMacro(target.id(), kMathMin, args);
          }
          case 2: {
            auto error = checkInvalidArgs(sf, "math.least()", args);
            if (error.has_value()) {
              return *error;
            }

            return sf->NewGlobalCallForMacro(target.id(), kMathMin, args);
          }
          default:
            auto error = checkInvalidArgs(sf, "math.least()", args);
            if (error.has_value()) {
              return *error;
            }

            return sf->NewGlobalCallForMacro(
                target.id(), kMathMin,
                {sf->NewList(sf->NextMacroId(macro_id), args)});
        }
      });
  absl::StatusOr<Macro> greatest = Macro::ReceiverVarArg(
      kGreatest, [](const std::shared_ptr<SourceFactory> &sf, int64_t macro_id,
                    const Expr &target, const std::vector<Expr> &args) {
        if (!isTargetNamespace(target)) {
          return Expr();
        }

        switch (args.size()) {
          case 0: {
            return sf->ReportError(
                target.id(), "math.greatest() requires at least one argument.");
          }
          case 1: {
            if (!isListLiteralWithValidArgs(args[0]) &&
                !isValidArgType(args[0])) {
              return sf->ReportError(
                  args[0].id(),
                  "math.greatest() invalid single argument value.");
            }

            return sf->NewGlobalCallForMacro(target.id(), kMathMax, args);
          }
          case 2: {
            auto error = checkInvalidArgs(sf, "math.greatest()", args);
            if (error.has_value()) {
              return *error;
            }
            return sf->NewGlobalCallForMacro(target.id(), kMathMax, args);
          }
          default: {
            auto error = checkInvalidArgs(sf, "math.greatest()", args);
            if (error.has_value()) {
              return *error;
            }

            return sf->NewGlobalCallForMacro(
                target.id(), kMathMax,
                {sf->NewList(sf->NextMacroId(macro_id), args)});
          }
        }
      });

  return {*least, *greatest};
}

}  // namespace cel::extensions
