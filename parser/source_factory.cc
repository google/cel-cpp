#include "parser/source_factory.h"

#include "google/protobuf/struct.pb.h"
#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "common/operators.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

using common::CelOperator;
using google::api::expr::v1alpha1::Expr;

SourceFactory::SourceFactory(const std::string& expression) : next_id_(1) {
  calcLineOffsets(expression);
}

int64_t SourceFactory::id(const antlr4::Token* token) {
  int64_t new_id = next_id_;
  positions_.emplace(
      new_id, SourceLocation{(int32_t)token->getLine(),
                             (int32_t)token->getCharPositionInLine(),
                             (int32_t)token->getStopIndex(), line_offsets_});
  next_id_ += 1;
  return new_id;
}

const SourceFactory::SourceLocation& SourceFactory::getSourceLocation(
    int64_t id) const {
  return positions_.at(id);
}

int64_t SourceFactory::id(antlr4::ParserRuleContext* ctx) {
  return id(ctx->getStart());
}

int64_t SourceFactory::id(const SourceLocation& location) {
  int64_t new_id = next_id_;
  positions_.emplace(new_id, location);
  next_id_ += 1;
  return new_id;
}

int64_t SourceFactory::nextMacroId(int64_t macro_id) {
  return id(getSourceLocation(macro_id));
}

Expr SourceFactory::newExpr(int64_t id) {
  Expr expr;
  expr.set_id(id);
  return expr;
}

Expr SourceFactory::newExpr(antlr4::ParserRuleContext* ctx) {
  return newExpr(id(ctx));
}

Expr SourceFactory::newExpr(const antlr4::Token* token) {
  return newExpr(id(token));
}

Expr SourceFactory::newGlobalCall(int64_t id, const std::string& function,
                                  const std::vector<Expr>& args) {
  Expr expr = newExpr(id);
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function(function);
  std::for_each(args.begin(), args.end(), [&call_expr](const Expr& e) {
    call_expr->add_args()->CopyFrom(e);
  });
  return expr;
}

Expr SourceFactory::newGlobalCallForMacro(int64_t macro_id,
                                          const std::string& function,
                                          const std::vector<Expr>& args) {
  return newGlobalCall(nextMacroId(macro_id), function, args);
}

Expr SourceFactory::newReceiverCall(int64_t id, const std::string& function,
                                    Expr& target,
                                    const std::vector<Expr>& args) {
  Expr expr = newExpr(id);
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function(function);
  call_expr->mutable_target()->CopyFrom(target);
  std::for_each(args.begin(), args.end(), [&call_expr](const Expr& e) {
    call_expr->add_args()->CopyFrom(e);
  });
  return expr;
}

Expr SourceFactory::newIdent(const antlr4::Token* token,
                             const std::string& ident_name) {
  Expr expr = newExpr(token);
  expr.mutable_ident_expr()->set_name(ident_name);
  return expr;
}

Expr SourceFactory::newIdentForMacro(int64_t macro_id,
                                     const std::string& ident_name) {
  Expr expr = newExpr(nextMacroId(macro_id));
  expr.mutable_ident_expr()->set_name(ident_name);
  return expr;
}

Expr SourceFactory::newSelect(
    ::cel_grammar::CelParser::SelectOrCallContext* ctx, Expr& operand,
    const std::string& field) {
  Expr expr = newExpr(ctx->op);
  auto select_expr = expr.mutable_select_expr();
  select_expr->mutable_operand()->CopyFrom(operand);
  select_expr->set_field(field);
  return expr;
}

Expr SourceFactory::newPresenceTestForMacro(int64_t macro_id, const Expr& operand,
                                            const std::string& field) {
  Expr expr = newExpr(nextMacroId(macro_id));
  auto select_expr = expr.mutable_select_expr();
  select_expr->mutable_operand()->CopyFrom(operand);
  select_expr->set_field(field);
  select_expr->set_test_only(true);
  return expr;
}

Expr SourceFactory::newObject(
    int64_t obj_id, std::string type_name,
    const std::vector<Expr::CreateStruct::Entry>& entries) {
  auto expr = newExpr(obj_id);
  auto struct_expr = expr.mutable_struct_expr();
  struct_expr->set_message_name(type_name);
  std::for_each(entries.begin(), entries.end(),
                [struct_expr](const Expr::CreateStruct::Entry& e) {
                  struct_expr->add_entries()->CopyFrom(e);
                });
  return expr;
}

Expr::CreateStruct::Entry SourceFactory::newObjectField(
    int64_t field_id, const std::string& field, const Expr& value) {
  Expr::CreateStruct::Entry entry;
  entry.set_id(field_id);
  entry.set_field_key(field);
  entry.mutable_value()->CopyFrom(value);
  return entry;
}

Expr SourceFactory::newComprehension(int64_t id, const std::string& iter_var,
                                     const Expr& iter_range,
                                     const std::string& accu_var,
                                     const Expr& accu_init,
                                     const Expr& condition, const Expr& step,
                                     const Expr& result) {
  Expr expr = newExpr(id);
  auto comp_expr = expr.mutable_comprehension_expr();
  comp_expr->set_iter_var(iter_var);
  comp_expr->mutable_iter_range()->CopyFrom(iter_range);
  comp_expr->set_accu_var(accu_var);
  comp_expr->mutable_accu_init()->CopyFrom(accu_init);
  comp_expr->mutable_loop_condition()->CopyFrom(condition);
  comp_expr->mutable_loop_step()->CopyFrom(step);
  comp_expr->mutable_result()->CopyFrom(result);
  return expr;
}

Expr SourceFactory::foldForMacro(int64_t macro_id, const std::string& iter_var,
                                 const Expr& iter_range,
                                 const std::string& accu_var,
                                 const Expr& accu_init, const Expr& condition,
                                 const Expr& step, const Expr& result) {
  return newComprehension(nextMacroId(macro_id), iter_var, iter_range, accu_var,
                          accu_init, condition, step, result);
}

Expr SourceFactory::newList(int64_t list_id, const std::vector<Expr>& elems) {
  auto expr = newExpr(list_id);
  auto list_expr = expr.mutable_list_expr();
  std::for_each(elems.begin(), elems.end(), [list_expr](const Expr& e) {
    list_expr->add_elements()->CopyFrom(e);
  });
  return expr;
}

Expr SourceFactory::newQuantifierExprForMacro(
    SourceFactory::QuantifierKind kind, int64_t macro_id, Expr* target,
    const std::vector<Expr>& args) {
  if (args.empty()) {
    return Expr();
  }
  if (!args[0].has_ident_expr()) {
    auto loc = getSourceLocation(args[0].id());
    return reportError(loc, "argument must be a simple name");
  }
  std::string v = args[0].ident_expr().name();

  // traditional variable name assigned to the fold accumulator variable.
  const std::string AccumulatorName = "__result__";

  auto accu_ident = [this, &macro_id, &AccumulatorName]() {
    return newIdentForMacro(macro_id, AccumulatorName);
  };

  Expr init;
  Expr condition;
  Expr step;
  Expr result;
  switch (kind) {
    case QUANTIFIER_ALL:
      init = newLiteralBoolForMacro(macro_id, true);
      condition = newGlobalCallForMacro(
          macro_id, CelOperator::NOT_STRICTLY_FALSE, {accu_ident()});
      step = newGlobalCallForMacro(macro_id, CelOperator::LOGICAL_AND,
                                   {accu_ident(), args[1]});
      result = accu_ident();
      break;

    case QUANTIFIER_EXISTS:
      init = newLiteralBoolForMacro(macro_id, false);
      condition = newGlobalCallForMacro(
          macro_id, CelOperator::NOT_STRICTLY_FALSE,
          {newGlobalCallForMacro(macro_id, CelOperator::LOGICAL_NOT,
                                 {accu_ident()})});
      step = newGlobalCallForMacro(macro_id, CelOperator::LOGICAL_OR,
                                   {accu_ident(), args[1]});
      result = accu_ident();
      break;

    case QUANTIFIER_EXISTS_ONE: {
      Expr zero_expr = newLiteralIntForMacro(macro_id, 0);
      Expr one_expr = newLiteralIntForMacro(macro_id, 1);
      init = zero_expr;
      condition = newGlobalCallForMacro(macro_id, CelOperator::LESS_EQUALS,
                                        {accu_ident(), one_expr});
      step = newGlobalCallForMacro(
          macro_id, CelOperator::CONDITIONAL,
          {args[1],
           newGlobalCallForMacro(macro_id, CelOperator::ADD,
                                 {accu_ident(), one_expr}),
           accu_ident()});
      result = newGlobalCallForMacro(macro_id, CelOperator::EQUALS,
                                     {accu_ident(), one_expr});
      break;
    }
  }
  return foldForMacro(macro_id, v, *target, AccumulatorName, init, condition,
                      step, result);
}

Expr SourceFactory::newFilterExprForMacro(int64_t macro_id, Expr* target,
                                          const std::vector<Expr>& args) {
  if (args.empty()) {
    return Expr();
  }
  if (!args[0].has_ident_expr()) {
    auto loc = getSourceLocation(args[0].id());
    return reportError(loc, "argument is not an identifier");
  }
  std::string v = args[0].ident_expr().name();

  // traditional variable name assigned to the fold accumulator variable.
  const std::string AccumulatorName = "__result__";

  Expr filter = args[1];
  Expr accu_expr = newIdentForMacro(macro_id, AccumulatorName);
  Expr init = newListForMacro(macro_id, {});
  Expr condition = newLiteralBoolForMacro(macro_id, true);
  Expr step =
      newGlobalCallForMacro(macro_id, CelOperator::ADD,
                            {accu_expr, newListForMacro(macro_id, {args[0]})});
  step = newGlobalCallForMacro(macro_id, CelOperator::CONDITIONAL,
                               {filter, step, accu_expr});
  return foldForMacro(macro_id, v, *target, AccumulatorName, init, condition,
                      step, accu_expr);
}

Expr SourceFactory::newListForMacro(int64_t macro_id,
                                    const std::vector<Expr>& elems) {
  return newList(nextMacroId(macro_id), elems);
}

Expr SourceFactory::newMap(
    int64_t map_id, const std::vector<Expr::CreateStruct::Entry>& entries) {
  auto expr = newExpr(map_id);
  auto struct_expr = expr.mutable_struct_expr();
  std::for_each(entries.begin(), entries.end(),
                [struct_expr](const Expr::CreateStruct::Entry& e) {
                  struct_expr->add_entries()->CopyFrom(e);
                });
  return expr;
}

Expr SourceFactory::newMapForMacro(int64_t macro_id, Expr* target,
                                   const std::vector<Expr>& args) {
  if (args.empty()) {
    return Expr();
  }
  if (!args[0].has_ident_expr()) {
    auto loc = getSourceLocation(args[0].id());
    return reportError(loc, "argument is not an identifier");
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

  Expr accu_expr = newIdentForMacro(macro_id, AccumulatorName);
  Expr init = newListForMacro(macro_id, {});
  Expr condition = newLiteralBoolForMacro(macro_id, true);
  Expr step = newGlobalCallForMacro(
      macro_id, CelOperator::ADD, {accu_expr, newListForMacro(macro_id, {fn})});
  if (has_filter) {
    step = newGlobalCallForMacro(macro_id, CelOperator::CONDITIONAL,
                                 {filter, step, accu_expr});
  }
  return foldForMacro(macro_id, v, *target, AccumulatorName, init, condition,
                      step, accu_expr);
}

Expr::CreateStruct::Entry SourceFactory::newMapEntry(int64_t entry_id,
                                                     const Expr& key,
                                                     const Expr& value) {
  Expr::CreateStruct::Entry entry;
  entry.set_id(entry_id);
  entry.mutable_map_key()->CopyFrom(key);
  entry.mutable_value()->CopyFrom(value);
  return entry;
}

Expr SourceFactory::newLiteralInt(antlr4::ParserRuleContext* ctx, int64_t value) {
  Expr expr = newExpr(ctx);
  expr.mutable_const_expr()->set_int64_value(value);
  return expr;
}

Expr SourceFactory::newLiteralIntForMacro(int64_t macro_id, int64_t value) {
  Expr expr = newExpr(nextMacroId(macro_id));
  expr.mutable_const_expr()->set_int64_value(value);
  return expr;
}

Expr SourceFactory::newLiteralUint(antlr4::ParserRuleContext* ctx,
                                   uint64_t value) {
  Expr expr = newExpr(ctx);
  expr.mutable_const_expr()->set_uint64_value(value);
  return expr;
}

Expr SourceFactory::newLiteralDouble(antlr4::ParserRuleContext* ctx,
                                     double value) {
  Expr expr = newExpr(ctx);
  expr.mutable_const_expr()->set_double_value(value);
  return expr;
}

Expr SourceFactory::newLiteralString(antlr4::ParserRuleContext* ctx,
                                     const std::string& s) {
  Expr expr = newExpr(ctx);
  expr.mutable_const_expr()->set_string_value(s);
  return expr;
}

Expr SourceFactory::newLiteralBytes(antlr4::ParserRuleContext* ctx,
                                    const std::string& b) {
  Expr expr = newExpr(ctx);
  expr.mutable_const_expr()->set_bytes_value(b);
  return expr;
}

Expr SourceFactory::newLiteralBool(antlr4::ParserRuleContext* ctx, bool b) {
  Expr expr = newExpr(ctx);
  expr.mutable_const_expr()->set_bool_value(b);
  return expr;
}

Expr SourceFactory::newLiteralBoolForMacro(int64_t macro_id, bool b) {
  Expr expr = newExpr(nextMacroId(macro_id));
  expr.mutable_const_expr()->set_bool_value(b);
  return expr;
}

Expr SourceFactory::newLiteralNull(antlr4::ParserRuleContext* ctx) {
  Expr expr = newExpr(ctx);
  expr.mutable_const_expr()->set_null_value(::google::protobuf::NULL_VALUE);
  return expr;
}

Expr SourceFactory::reportError(antlr4::ParserRuleContext* ctx,
                                const std::string& msg) {
  Expr expr = newExpr(ctx);
  errors_.emplace_back(msg, positions_.at(expr.id()));
  return expr;
}

Expr SourceFactory::reportError(int32_t line, int32_t col, const std::string& msg) {
  SourceLocation loc(line, col, /*offset_end=*/-1, line_offsets_);
  errors_.emplace_back(msg, loc);
  return newExpr(id(loc));
}

Expr SourceFactory::reportError(const SourceFactory::SourceLocation& loc,
                                const std::string& msg) {
  errors_.emplace_back(msg, loc);
  return newExpr(id(loc));
}

bool SourceFactory::isReserved(const std::string& ident_name) {
  static std::vector<std::string> reserved_words = {
      "as",        "break", "const",  "continue", "else", "false", "for",
      "function",  "if",    "import", "in",       "let",  "loop",  "package",
      "namespace", "null",  "return", "true",     "var",  "void",  "while"};
  return std::find(reserved_words.begin(), reserved_words.end(), ident_name) !=
         reserved_words.end();
}

google::api::expr::v1alpha1::SourceInfo SourceFactory::sourceInfo() const {
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
  return source_info;
}

EnrichedSourceInfo SourceFactory::enrichedSourceInfo() const {
  std::map<int64_t, std::pair<int32_t, int32_t>> offset;
  std::for_each(
      positions_.begin(), positions_.end(),
      [&offset](const std::pair<int64_t, SourceLocation>& loc) {
        offset.insert({loc.first, {loc.second.offset, loc.second.offset_end}});
      });
  return EnrichedSourceInfo(offset);
}

const std::vector<int32_t>& SourceFactory::line_offsets() const {
  return line_offsets_;
}

void SourceFactory::calcLineOffsets(const std::string& expression) {
  std::vector<absl::string_view> lines = absl::StrSplit(expression, '\n');
  int offset = 0;
  line_offsets_.resize(lines.size());
  for (size_t i = 0; i < lines.size(); ++i) {
    offset += lines[i].size() + 1;
    line_offsets_[i] = offset;
  }
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
