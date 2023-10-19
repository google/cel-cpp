// Copyright 2021 Google LLC
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

#include "parser/macro.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/operators.h"
#include "internal/lexis.h"
#include "internal/no_destructor.h"
#include "parser/source_factory.h"

namespace cel {

namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::common::CelOperator;

absl::StatusOr<Macro> MakeMacro(absl::string_view name, size_t argument_count,
                                MacroExpander expander,
                                bool is_receiver_style) {
  if (!internal::LexisIsIdentifier(name)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Macro function name \"", name, "\" is not a valid identifier"));
  }
  if (!expander) {
    return absl::InvalidArgumentError(
        absl::StrCat("Macro expander for \"", name, "\" cannot be empty"));
  }
  return Macro(name, argument_count, std::move(expander), is_receiver_style);
}

absl::StatusOr<Macro> MakeMacro(absl::string_view name, MacroExpander expander,
                                bool is_receiver_style) {
  if (!internal::LexisIsIdentifier(name)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Macro function name \"", name, "\" is not a valid identifier"));
  }
  if (!expander) {
    return absl::InvalidArgumentError(
        absl::StrCat("Macro expander for \"", name, "\" cannot be empty"));
  }
  return Macro(name, std::move(expander), is_receiver_style);
}

}  // namespace

absl::StatusOr<Macro> Macro::Global(absl::string_view name,
                                    size_t argument_count,
                                    MacroExpander expander) {
  return MakeMacro(name, argument_count, std::move(expander), false);
}

absl::StatusOr<Macro> Macro::GlobalVarArg(absl::string_view name,
                                          MacroExpander expander) {
  return MakeMacro(name, std::move(expander), false);
}

absl::StatusOr<Macro> Macro::Receiver(absl::string_view name,
                                      size_t argument_count,
                                      MacroExpander expander) {
  return MakeMacro(name, argument_count, std::move(expander), true);
}

absl::StatusOr<Macro> Macro::ReceiverVarArg(absl::string_view name,
                                            MacroExpander expander) {
  return MakeMacro(name, std::move(expander), true);
}

std::vector<Macro> Macro::AllMacros() {
  return {HasMacro(),  AllMacro(),  ExistsMacro(), ExistsOneMacro(),
          Map2Macro(), Map3Macro(), FilterMacro()};
}

Macro HasMacro() {
  // The macro "has(m.f)" which tests the presence of a field, avoiding the
  // need to specify the field as a string.
  static const internal::NoDestructor<Macro> macro(
      CelOperator::HAS, 1,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        if (!args.empty() && args[0].has_select_expr()) {
          const auto& sel_expr = args[0].select_expr();
          return sf->NewPresenceTestForMacro(macro_id, sel_expr.operand(),
                                             sel_expr.field());
        } else {
          // error
          return Expr();
        }
      });
  return macro.get();
}

Macro AllMacro() {
  // The macro "range.all(var, predicate)", which is true if for all
  // elements in range the predicate holds.
  static const internal::NoDestructor<Macro> macro(
      CelOperator::ALL, 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        return sf->NewQuantifierExprForMacro(SourceFactory::QUANTIFIER_ALL,
                                             macro_id, target, args);
      },
      /* receiver style*/ true);
  return macro.get();
}

Macro ExistsMacro() {
  // The macro "range.exists(var, predicate)", which is true if for at least
  // one element in range the predicate holds.
  static const internal::NoDestructor<Macro> macro(
      CelOperator::EXISTS, 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        return sf->NewQuantifierExprForMacro(SourceFactory::QUANTIFIER_EXISTS,
                                             macro_id, target, args);
      },
      /* receiver style*/ true);
  return macro.get();
}

Macro ExistsOneMacro() {
  // The macro "range.exists_one(var, predicate)", which is true if for
  // exactly one element in range the predicate holds.
  static const internal::NoDestructor<Macro> macro(
      CelOperator::EXISTS_ONE, 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        return sf->NewQuantifierExprForMacro(
            SourceFactory::QUANTIFIER_EXISTS_ONE, macro_id, target, args);
      },
      /* receiver style*/ true);
  return macro.get();
}

Macro Map2Macro() {
  // The macro "range.map(var, function)", applies the function to the vars
  // in the range.
  static const internal::NoDestructor<Macro> macro(
      CelOperator::MAP, 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        return sf->NewMapForMacro(macro_id, target, args);
      },
      /* receiver style*/ true);
  return macro.get();
}

Macro Map3Macro() {
  // The macro "range.map(var, predicate, function)", applies the function
  // to the vars in the range for which the predicate holds true. The other
  // variables are filtered out.
  static const internal::NoDestructor<Macro> macro(
      CelOperator::MAP, 3,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        return sf->NewMapForMacro(macro_id, target, args);
      },
      /* receiver style*/ true);
  return macro.get();
}

Macro FilterMacro() {
  // The macro "range.filter(var, predicate)", filters out the variables for
  // which the predicate is false.
  static const internal::NoDestructor<Macro> macro(
      CelOperator::FILTER, 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) {
        return sf->NewFilterExprForMacro(macro_id, target, args);
      },
      /* receiver style*/ true);
  return macro.get();
}

Macro OptMapMacro() {
  static const internal::NoDestructor<Macro> macro(
      "optMap", 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) -> Expr {
        if (args.size() != 2) {
          return sf->ReportError(args[0].id(), "optMap() requires 2 arguments");
        }
        if (!args[0].has_ident_expr()) {
          return sf->ReportError(
              args[0].id(),
              "optMap() variable name must be a simple identifier");
        }
        const auto& var_name = args[0].ident_expr().name();
        const auto& map_expr = args[1];

        std::vector<Expr> call_args;
        call_args.resize(3);
        call_args[0] =
            sf->NewReceiverCallForMacro(macro_id, "hasValue", target, {});
        auto iter_range = sf->NewListForMacro(macro_id, {});
        auto accu_init =
            sf->NewReceiverCallForMacro(macro_id, "value", target, {});
        auto condition = sf->NewLiteralBoolForMacro(macro_id, false);
        auto step = sf->NewIdentForMacro(macro_id, var_name);
        const auto& result = map_expr;
        auto fold = sf->FoldForMacro(macro_id, "#unused", iter_range, var_name,
                                     accu_init, condition, step, result);
        call_args[1] =
            sf->NewGlobalCallForMacro(macro_id, "optional.of", {fold});
        call_args[2] = sf->NewGlobalCallForMacro(macro_id, "optional.none", {});
        return sf->NewGlobalCallForMacro(macro_id, CelOperator::CONDITIONAL,
                                         call_args);
      },
      true);
  return macro.get();
}

Macro OptFlatMapMacro() {
  static const internal::NoDestructor<Macro> macro(
      "optFlatMap", 2,
      [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
         const Expr& target, const std::vector<Expr>& args) -> Expr {
        if (args.size() != 2) {
          return sf->ReportError(args[0].id(),
                                 "optFlatMap() requires 2 arguments");
        }
        if (!args[0].has_ident_expr()) {
          return sf->ReportError(
              args[0].id(),
              "optFlatMap() variable name must be a simple identifier");
        }
        const auto& var_name = args[0].ident_expr().name();
        const auto& map_expr = args[1];
        std::vector<Expr> call_args;
        call_args.resize(3);
        call_args[0] =
            sf->NewReceiverCallForMacro(macro_id, "hasValue", target, {});
        auto iter_range = sf->NewListForMacro(macro_id, {});
        auto accu_init =
            sf->NewReceiverCallForMacro(macro_id, "value", target, {});
        auto condition = sf->NewLiteralBoolForMacro(macro_id, false);
        auto step = sf->NewIdentForMacro(macro_id, var_name);
        const auto& result = map_expr;
        call_args[1] =
            sf->FoldForMacro(macro_id, "#unused", iter_range, var_name,
                             accu_init, condition, step, result);
        call_args[2] = sf->NewGlobalCallForMacro(macro_id, "optional.none", {});
        return sf->NewGlobalCallForMacro(macro_id, CelOperator::CONDITIONAL,
                                         call_args);
      },
      true);
  return macro.get();
}

}  // namespace cel
