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

#include "parser/source_factory.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "common/operators.h"

namespace google::api::expr::parser {
namespace {

const int kMaxErrorsToReport = 100;

using common::CelOperator;
using google::api::expr::v1alpha1::Expr;

int32_t PositiveOrMax(int32_t value) {
  return value >= 0 ? value : std::numeric_limits<int32_t>::max();
}

}  // namespace

SourceFactory::SourceFactory(absl::string_view expression)
    : next_id_(1), num_errors_(0) {
  CalcLineOffsets(expression);
}

int64_t SourceFactory::Id(const antlr4::Token* token) {
  int64_t new_id = next_id_;
  positions_.emplace(
      new_id, SourceLocation{
                  static_cast<int32_t>(token->getLine()),
                  static_cast<int32_t>(token->getCharPositionInLine()),
                  static_cast<int32_t>(token->getStopIndex()), line_offsets_});
  next_id_ += 1;
  return new_id;
}

const SourceFactory::SourceLocation& SourceFactory::GetSourceLocation(
    int64_t id) const {
  return positions_.at(id);
}

const SourceFactory::SourceLocation SourceFactory::NoLocation() {
  return SourceLocation(-1, -1, -1, {});
}

int64_t SourceFactory::Id(antlr4::ParserRuleContext* ctx) {
  return Id(ctx->getStart());
}

int64_t SourceFactory::Id(const SourceLocation& location) {
  int64_t new_id = next_id_;
  positions_.emplace(new_id, location);
  next_id_ += 1;
  return new_id;
}

int64_t SourceFactory::NextMacroId(int64_t macro_id) {
  return Id(GetSourceLocation(macro_id));
}

Expr SourceFactory::NewExpr(int64_t id) {
  Expr expr;
  expr.set_id(id);
  return expr;
}

Expr SourceFactory::NewExpr(antlr4::ParserRuleContext* ctx) {
  return NewExpr(Id(ctx));
}

Expr SourceFactory::NewExpr(const antlr4::Token* token) {
  return NewExpr(Id(token));
}

Expr SourceFactory::NewGlobalCall(int64_t id, const std::string& function,
                                  const std::vector<Expr>& args) {
  Expr expr = NewExpr(id);
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function(function);
  std::for_each(args.begin(), args.end(),
                [&call_expr](const Expr& e) { *call_expr->add_args() = e; });
  return expr;
}

Expr SourceFactory::NewGlobalCallForMacro(int64_t macro_id,
                                          const std::string& function,
                                          const std::vector<Expr>& args) {
  return NewGlobalCall(NextMacroId(macro_id), function, args);
}

Expr SourceFactory::NewReceiverCall(int64_t id, const std::string& function,
                                    const Expr& target,
                                    const std::vector<Expr>& args) {
  Expr expr = NewExpr(id);
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function(function);
  *call_expr->mutable_target() = target;
  std::for_each(args.begin(), args.end(),
                [&call_expr](const Expr& e) { *call_expr->add_args() = e; });
  return expr;
}

Expr SourceFactory::NewIdent(const antlr4::Token* token,
                             const std::string& ident_name) {
  Expr expr = NewExpr(token);
  expr.mutable_ident_expr()->set_name(ident_name);
  return expr;
}

Expr SourceFactory::NewIdentForMacro(int64_t macro_id,
                                     const std::string& ident_name) {
  Expr expr = NewExpr(NextMacroId(macro_id));
  expr.mutable_ident_expr()->set_name(ident_name);
  return expr;
}

Expr SourceFactory::NewSelect(
    ::cel_parser_internal::CelParser::SelectOrCallContext* ctx, Expr& operand,
    const std::string& field) {
  Expr expr = NewExpr(ctx->op);
  auto select_expr = expr.mutable_select_expr();
  *select_expr->mutable_operand() = operand;
  select_expr->set_field(field);
  return expr;
}

Expr SourceFactory::NewPresenceTestForMacro(int64_t macro_id,
                                            const Expr& operand,
                                            const std::string& field) {
  Expr expr = NewExpr(NextMacroId(macro_id));
  auto select_expr = expr.mutable_select_expr();
  *select_expr->mutable_operand() = operand;
  select_expr->set_field(field);
  select_expr->set_test_only(true);
  return expr;
}

Expr SourceFactory::NewObject(
    int64_t obj_id, const std::string& type_name,
    const std::vector<Expr::CreateStruct::Entry>& entries) {
  auto expr = NewExpr(obj_id);
  auto struct_expr = expr.mutable_struct_expr();
  struct_expr->set_message_name(type_name);
  std::for_each(entries.begin(), entries.end(),
                [struct_expr](const Expr::CreateStruct::Entry& e) {
                  struct_expr->add_entries()->CopyFrom(e);
                });
  return expr;
}

Expr::CreateStruct::Entry SourceFactory::NewObjectField(
    int64_t field_id, const std::string& field, const Expr& value) {
  Expr::CreateStruct::Entry entry;
  entry.set_id(field_id);
  entry.set_field_key(field);
  *entry.mutable_value() = value;
  return entry;
}

Expr SourceFactory::NewComprehension(int64_t id, const std::string& iter_var,
                                     const Expr& iter_range,
                                     const std::string& accu_var,
                                     const Expr& accu_init,
                                     const Expr& condition, const Expr& step,
                                     const Expr& result) {
  Expr expr = NewExpr(id);
  auto comp_expr = expr.mutable_comprehension_expr();
  comp_expr->set_iter_var(iter_var);
  *comp_expr->mutable_iter_range() = iter_range;
  comp_expr->set_accu_var(accu_var);
  *comp_expr->mutable_accu_init() = accu_init;
  *comp_expr->mutable_loop_condition() = condition;
  *comp_expr->mutable_loop_step() = step;
  *comp_expr->mutable_result() = result;
  return expr;
}

Expr SourceFactory::FoldForMacro(int64_t macro_id, const std::string& iter_var,
                                 const Expr& iter_range,
                                 const std::string& accu_var,
                                 const Expr& accu_init, const Expr& condition,
                                 const Expr& step, const Expr& result) {
  return NewComprehension(NextMacroId(macro_id), iter_var, iter_range, accu_var,
                          accu_init, condition, step, result);
}

Expr SourceFactory::NewList(int64_t list_id, const std::vector<Expr>& elems) {
  auto expr = NewExpr(list_id);
  auto list_expr = expr.mutable_list_expr();
  std::for_each(elems.begin(), elems.end(),
                [list_expr](const Expr& e) { *list_expr->add_elements() = e; });
  return expr;
}

Expr SourceFactory::NewQuantifierExprForMacro(
    SourceFactory::QuantifierKind kind, int64_t macro_id, const Expr& target,
    const std::vector<Expr>& args) {
  if (args.empty()) {
    return Expr();
  }
  if (!args[0].has_ident_expr()) {
    auto loc = GetSourceLocation(args[0].id());
    return ReportError(loc, "argument must be a simple name");
  }
  std::string v = args[0].ident_expr().name();

  // traditional variable name assigned to the fold accumulator variable.
  const std::string AccumulatorName = "__result__";

  auto accu_ident = [this, &macro_id, &AccumulatorName]() {
    return NewIdentForMacro(macro_id, AccumulatorName);
  };

  Expr init;
  Expr condition;
  Expr step;
  Expr result;
  switch (kind) {
    case QUANTIFIER_ALL:
      init = NewLiteralBoolForMacro(macro_id, true);
      condition = NewGlobalCallForMacro(
          macro_id, CelOperator::NOT_STRICTLY_FALSE, {accu_ident()});
      step = NewGlobalCallForMacro(macro_id, CelOperator::LOGICAL_AND,
                                   {accu_ident(), args[1]});
      result = accu_ident();
      break;

    case QUANTIFIER_EXISTS:
      init = NewLiteralBoolForMacro(macro_id, false);
      condition = NewGlobalCallForMacro(
          macro_id, CelOperator::NOT_STRICTLY_FALSE,
          {NewGlobalCallForMacro(macro_id, CelOperator::LOGICAL_NOT,
                                 {accu_ident()})});
      step = NewGlobalCallForMacro(macro_id, CelOperator::LOGICAL_OR,
                                   {accu_ident(), args[1]});
      result = accu_ident();
      break;

    case QUANTIFIER_EXISTS_ONE: {
      Expr zero_expr = NewLiteralIntForMacro(macro_id, 0);
      Expr one_expr = NewLiteralIntForMacro(macro_id, 1);
      init = zero_expr;
      condition = NewLiteralBoolForMacro(macro_id, true);
      step = NewGlobalCallForMacro(
          macro_id, CelOperator::CONDITIONAL,
          {args[1],
           NewGlobalCallForMacro(macro_id, CelOperator::ADD,
                                 {accu_ident(), one_expr}),
           accu_ident()});
      result = NewGlobalCallForMacro(macro_id, CelOperator::EQUALS,
                                     {accu_ident(), one_expr});
      break;
    }
  }
  return FoldForMacro(macro_id, v, target, AccumulatorName, init, condition,
                      step, result);
}

Expr SourceFactory::BuildArgForMacroCall(const Expr& expr) {
  if (macro_calls_.find(expr.id()) != macro_calls_.end()) {
    Expr result_expr;
    result_expr.set_id(expr.id());
    return result_expr;
  }
  // Call expression could have args or sub-args that are also macros found in
  // macro_calls.
  if (expr.has_call_expr()) {
    Expr result_expr;
    result_expr.set_id(expr.id());
    auto mutable_expr = result_expr.mutable_call_expr();
    mutable_expr->set_function(expr.call_expr().function());
    if (expr.call_expr().has_target()) {
      *mutable_expr->mutable_target() =
          BuildArgForMacroCall(expr.call_expr().target());
    }
    for (const auto& arg : expr.call_expr().args()) {
      // Iterate the AST from `expr` recursively looking for macros. Because we
      // are at most starting from the top level macro, this recursion is
      // bounded by the size of the AST. This means that the depth check on the
      // AST during parsing will catch recursion overflows before we get to
      // here.
      *mutable_expr->mutable_args()->Add() = BuildArgForMacroCall(arg);
    }
    return result_expr;
  }
  if (expr.has_list_expr()) {
    Expr result_expr;
    result_expr.set_id(expr.id());
    const auto& list_expr = expr.list_expr();
    auto mutable_list_expr = result_expr.mutable_list_expr();
    for (const auto& elem : list_expr.elements()) {
      *mutable_list_expr->mutable_elements()->Add() =
          BuildArgForMacroCall(elem);
    }
    return result_expr;
  }
  return expr;
}

void SourceFactory::AddMacroCall(int64_t macro_id, const Expr& target,
                                 const std::vector<Expr>& args,
                                 std::string function) {
  Expr macro_call;
  auto mutable_macro_call = macro_call.mutable_call_expr();
  mutable_macro_call->set_function(function);

  // Populating empty targets can cause erros when iterating the macro_calls
  // expressions, such as the expression_printer in testing.
  if (target.expr_kind_case() != Expr::ExprKindCase::EXPR_KIND_NOT_SET) {
    Expr expr;
    if (macro_calls_.find(target.id()) != macro_calls_.end()) {
      expr.set_id(target.id());
    } else {
      expr = BuildArgForMacroCall(target);
    }
    *mutable_macro_call->mutable_target() = expr;
  }

  for (const auto& arg : args) {
    *mutable_macro_call->mutable_args()->Add() = BuildArgForMacroCall(arg);
  }
  macro_calls_.emplace(macro_id, macro_call);
}

Expr SourceFactory::NewFilterExprForMacro(int64_t macro_id, const Expr& target,
                                          const std::vector<Expr>& args) {
  if (args.empty()) {
    return Expr();
  }
  if (!args[0].has_ident_expr()) {
    auto loc = GetSourceLocation(args[0].id());
    return ReportError(loc, "argument is not an identifier");
  }
  std::string v = args[0].ident_expr().name();

  // traditional variable name assigned to the fold accumulator variable.
  const std::string AccumulatorName = "__result__";

  Expr filter = args[1];
  Expr accu_expr = NewIdentForMacro(macro_id, AccumulatorName);
  Expr init = NewListForMacro(macro_id, {});
  Expr condition = NewLiteralBoolForMacro(macro_id, true);
  Expr step =
      NewGlobalCallForMacro(macro_id, CelOperator::ADD,
                            {accu_expr, NewListForMacro(macro_id, {args[0]})});
  step = NewGlobalCallForMacro(macro_id, CelOperator::CONDITIONAL,
                               {filter, step, accu_expr});
  return FoldForMacro(macro_id, v, target, AccumulatorName, init, condition,
                      step, accu_expr);
}

Expr SourceFactory::NewListForMacro(int64_t macro_id,
                                    const std::vector<Expr>& elems) {
  return NewList(NextMacroId(macro_id), elems);
}

Expr SourceFactory::NewMap(
    int64_t map_id, const std::vector<Expr::CreateStruct::Entry>& entries) {
  auto expr = NewExpr(map_id);
  auto struct_expr = expr.mutable_struct_expr();
  std::for_each(entries.begin(), entries.end(),
                [struct_expr](const Expr::CreateStruct::Entry& e) {
                  struct_expr->add_entries()->CopyFrom(e);
                });
  return expr;
}

Expr SourceFactory::NewMapForMacro(int64_t macro_id, const Expr& target,
                                   const std::vector<Expr>& args) {
  if (args.empty()) {
    return Expr();
  }
  if (!args[0].has_ident_expr()) {
    auto loc = GetSourceLocation(args[0].id());
    return ReportError(loc, "argument is not an identifier");
  }
  std::string v = args[0].ident_expr().name();

  Expr fn;
  Expr filter;
  bool has_filter = false;
  if (args.size() == 3) {
    filter = args[1];
    has_filter = true;
    fn = args[2];
  } else {
    fn = args[1];
  }

  // traditional variable name assigned to the fold accumulator variable.
  const std::string AccumulatorName = "__result__";

  Expr accu_expr = NewIdentForMacro(macro_id, AccumulatorName);
  Expr init = NewListForMacro(macro_id, {});
  Expr condition = NewLiteralBoolForMacro(macro_id, true);
  Expr step = NewGlobalCallForMacro(
      macro_id, CelOperator::ADD, {accu_expr, NewListForMacro(macro_id, {fn})});
  if (has_filter) {
    step = NewGlobalCallForMacro(macro_id, CelOperator::CONDITIONAL,
                                 {filter, step, accu_expr});
  }
  return FoldForMacro(macro_id, v, target, AccumulatorName, init, condition,
                      step, accu_expr);
}

Expr::CreateStruct::Entry SourceFactory::NewMapEntry(int64_t entry_id,
                                                     const Expr& key,
                                                     const Expr& value) {
  Expr::CreateStruct::Entry entry;
  entry.set_id(entry_id);
  *entry.mutable_map_key() = key;
  *entry.mutable_value() = value;
  return entry;
}

Expr SourceFactory::NewLiteralInt(antlr4::ParserRuleContext* ctx,
                                  int64_t value) {
  Expr expr = NewExpr(ctx);
  expr.mutable_const_expr()->set_int64_value(value);
  return expr;
}

Expr SourceFactory::NewLiteralIntForMacro(int64_t macro_id, int64_t value) {
  Expr expr = NewExpr(NextMacroId(macro_id));
  expr.mutable_const_expr()->set_int64_value(value);
  return expr;
}

Expr SourceFactory::NewLiteralUint(antlr4::ParserRuleContext* ctx,
                                   uint64_t value) {
  Expr expr = NewExpr(ctx);
  expr.mutable_const_expr()->set_uint64_value(value);
  return expr;
}

Expr SourceFactory::NewLiteralDouble(antlr4::ParserRuleContext* ctx,
                                     double value) {
  Expr expr = NewExpr(ctx);
  expr.mutable_const_expr()->set_double_value(value);
  return expr;
}

Expr SourceFactory::NewLiteralString(antlr4::ParserRuleContext* ctx,
                                     const std::string& s) {
  Expr expr = NewExpr(ctx);
  expr.mutable_const_expr()->set_string_value(s);
  return expr;
}

Expr SourceFactory::NewLiteralBytes(antlr4::ParserRuleContext* ctx,
                                    const std::string& b) {
  Expr expr = NewExpr(ctx);
  expr.mutable_const_expr()->set_bytes_value(b);
  return expr;
}

Expr SourceFactory::NewLiteralBool(antlr4::ParserRuleContext* ctx, bool b) {
  Expr expr = NewExpr(ctx);
  expr.mutable_const_expr()->set_bool_value(b);
  return expr;
}

Expr SourceFactory::NewLiteralBoolForMacro(int64_t macro_id, bool b) {
  Expr expr = NewExpr(NextMacroId(macro_id));
  expr.mutable_const_expr()->set_bool_value(b);
  return expr;
}

Expr SourceFactory::NewLiteralNull(antlr4::ParserRuleContext* ctx) {
  Expr expr = NewExpr(ctx);
  expr.mutable_const_expr()->set_null_value(::google::protobuf::NULL_VALUE);
  return expr;
}

Expr SourceFactory::ReportError(antlr4::ParserRuleContext* ctx,
                                absl::string_view msg) {
  num_errors_ += 1;
  Expr expr = NewExpr(ctx);
  if (errors_truncated_.size() < kMaxErrorsToReport) {
    errors_truncated_.emplace_back(std::string(msg), positions_.at(expr.id()));
  }
  return expr;
}

Expr SourceFactory::ReportError(int32_t line, int32_t col,
                                absl::string_view msg) {
  num_errors_ += 1;
  SourceLocation loc(line, col, /*offset_end=*/-1, line_offsets_);
  if (errors_truncated_.size() < kMaxErrorsToReport) {
    errors_truncated_.emplace_back(std::string(msg), loc);
  }
  return NewExpr(Id(loc));
}

Expr SourceFactory::ReportError(const SourceFactory::SourceLocation& loc,
                                absl::string_view msg) {
  num_errors_ += 1;
  if (errors_truncated_.size() < kMaxErrorsToReport) {
    errors_truncated_.emplace_back(std::string(msg), loc);
  }
  return NewExpr(Id(loc));
}

std::string SourceFactory::ErrorMessage(absl::string_view description,
                                        absl::string_view expression) const {
  // Errors are collected as they are encountered, not by their location within
  // the source. To have a more stable error message as implementation
  // details change, we sort the collected errors by their source location
  // first.

  // Use pointer arithmetic to avoid making unnecessary copies of Error when
  // sorting.
  std::vector<const Error*> errors_sorted;
  errors_sorted.reserve(errors_truncated_.size());
  for (auto& error : errors_truncated_) {
    errors_sorted.push_back(&error);
  }
  std::stable_sort(errors_sorted.begin(), errors_sorted.end(),
                   [](const Error* lhs, const Error* rhs) {
                     // SourceLocation::noLocation uses -1 and we ideally want
                     // those to be last.
                     auto lhs_line = PositiveOrMax(lhs->location.line);
                     auto lhs_col = PositiveOrMax(lhs->location.col);
                     auto rhs_line = PositiveOrMax(rhs->location.line);
                     auto rhs_col = PositiveOrMax(rhs->location.col);

                     return lhs_line < rhs_line ||
                            (lhs_line == rhs_line && lhs_col < rhs_col);
                   });

  // Build the summary error message using the sorted errors.
  bool errors_truncated = num_errors_ > kMaxErrorsToReport;
  std::vector<std::string> messages;
  messages.reserve(
      errors_sorted.size() +
      errors_truncated);  // Reserve space for the transform and an
                          // additional element when truncation occurs.
  std::transform(
      errors_sorted.begin(), errors_sorted.end(), std::back_inserter(messages),
      [this, &description, &expression](const SourceFactory::Error* error) {
        std::string s = absl::StrFormat(
            "ERROR: %s:%zu:%zu: %s", description, error->location.line,
            // add one to the 0-based column
            error->location.col + 1, error->message);
        std::string snippet = GetSourceLine(error->location.line, expression);
        std::string::size_type pos = 0;
        while ((pos = snippet.find('\t', pos)) != std::string::npos) {
          snippet.replace(pos, 1, " ");
        }
        std::string src_line = "\n | " + snippet;
        std::string ind_line = "\n | ";
        for (int i = 0; i < error->location.col; ++i) {
          ind_line += ".";
        }
        ind_line += "^";
        s += src_line + ind_line;
        return s;
      });
  if (errors_truncated) {
    messages.emplace_back(absl::StrCat(num_errors_ - kMaxErrorsToReport,
                                       " more errors were truncated."));
  }
  return absl::StrJoin(messages, "\n");
}

bool SourceFactory::IsReserved(absl::string_view ident_name) {
  static const auto* reserved_words = new absl::flat_hash_set<std::string>(
      {"as",        "break", "const",  "continue", "else", "false", "for",
       "function",  "if",    "import", "in",       "let",  "loop",  "package",
       "namespace", "null",  "return", "true",     "var",  "void",  "while"});
  return reserved_words->find(ident_name) != reserved_words->end();
}

google::api::expr::v1alpha1::SourceInfo SourceFactory::source_info() const {
  google::api::expr::v1alpha1::SourceInfo source_info;
  source_info.set_location("<input>");
  auto positions = source_info.mutable_positions();
  std::for_each(positions_.begin(), positions_.end(),
                [positions](const std::pair<int64_t, SourceLocation>& loc) {
                  positions->insert({loc.first, loc.second.offset});
                });
  std::for_each(
      line_offsets_.begin(), line_offsets_.end(),
      [&source_info](int32_t offset) { source_info.add_line_offsets(offset); });
  std::for_each(macro_calls_.begin(), macro_calls_.end(),
                [&source_info](const std::pair<int64_t, Expr>& macro_call) {
                  source_info.mutable_macro_calls()->insert(
                      {macro_call.first, macro_call.second});
                });
  return source_info;
}

EnrichedSourceInfo SourceFactory::enriched_source_info() const {
  std::map<int64_t, std::pair<int32_t, int32_t>> offset;
  std::for_each(
      positions_.begin(), positions_.end(),
      [&offset](const std::pair<int64_t, SourceLocation>& loc) {
        offset.insert({loc.first, {loc.second.offset, loc.second.offset_end}});
      });
  return EnrichedSourceInfo(std::move(offset));
}

void SourceFactory::CalcLineOffsets(absl::string_view expression) {
  std::vector<absl::string_view> lines = absl::StrSplit(expression, '\n');
  int offset = 0;
  line_offsets_.resize(lines.size());
  for (size_t i = 0; i < lines.size(); ++i) {
    offset += lines[i].size() + 1;
    line_offsets_[i] = offset;
  }
}

absl::optional<int32_t> SourceFactory::FindLineOffset(int32_t line) const {
  // note that err.line is 1-based,
  // while we need the 0-based index
  if (line == 1) {
    return 0;
  } else if (line > 1 && line <= static_cast<int32_t>(line_offsets_.size())) {
    return line_offsets_[line - 2];
  }
  return {};
}

std::string SourceFactory::GetSourceLine(int32_t line,
                                         absl::string_view expression) const {
  auto char_start = FindLineOffset(line);
  if (!char_start) {
    return "";
  }
  auto char_end = FindLineOffset(line + 1);
  if (char_end) {
    return std::string(
        expression.substr(*char_start, *char_end - *char_end - 1));
  } else {
    return std::string(expression.substr(*char_start));
  }
}

}  // namespace google::api::expr::parser
