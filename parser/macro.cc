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

#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "common/operators.h"
#include "internal/lexis.h"
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
  return {
      // The macro "has(m.f)" which tests the presence of a field, avoiding the
      // need to specify the field as a string.
      Macro(CelOperator::HAS, 1,
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
            }),

      // The macro "range.all(var, predicate)", which is true if for all
      // elements
      // in range the predicate holds.
      Macro(
          CelOperator::ALL, 2,
          [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
             const Expr& target, const std::vector<Expr>& args) {
            return sf->NewQuantifierExprForMacro(SourceFactory::QUANTIFIER_ALL,
                                                 macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.exists(var, predicate)", which is true if for at least
      // one element in range the predicate holds.
      Macro(
          CelOperator::EXISTS, 2,
          [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
             const Expr& target, const std::vector<Expr>& args) {
            return sf->NewQuantifierExprForMacro(
                SourceFactory::QUANTIFIER_EXISTS, macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.exists_one(var, predicate)", which is true if for
      // exactly one element in range the predicate holds.
      Macro(
          CelOperator::EXISTS_ONE, 2,
          [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
             const Expr& target, const std::vector<Expr>& args) {
            return sf->NewQuantifierExprForMacro(
                SourceFactory::QUANTIFIER_EXISTS_ONE, macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.map(var, function)", applies the function to the vars
      // in
      // the range.
      Macro(
          CelOperator::MAP, 2,
          [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
             const Expr& target, const std::vector<Expr>& args) {
            return sf->NewMapForMacro(macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.map(var, predicate, function)", applies the function
      // to
      // the vars in the range for which the predicate holds true. The other
      // variables are filtered out.
      Macro(
          CelOperator::MAP, 3,
          [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
             const Expr& target, const std::vector<Expr>& args) {
            return sf->NewMapForMacro(macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.filter(var, predicate)", filters out the variables for
      // which the
      // predicate is false.
      Macro(
          CelOperator::FILTER, 2,
          [](const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
             const Expr& target, const std::vector<Expr>& args) {
            return sf->NewFilterExprForMacro(macro_id, target, args);
          },
          /* receiver style*/ true),
  };
}

}  // namespace cel
