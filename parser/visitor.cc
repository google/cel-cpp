#include "parser/visitor.h"

#include <memory>

#include "google/protobuf/struct.pb.h"
#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "common/escaping.h"
#include "common/operators.h"
#include "parser/balancer.h"
#include "parser/source_factory.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

using common::CelOperator;
using common::ReverseLookupOperator;

using ::cel_grammar::CelParser;
using google::api::expr::v1alpha1::Expr;

ParserVisitor::ParserVisitor(const std::string& description,
                             const std::string& expression,
                             const std::vector<Macro>& macros)
    : description_(description),
      expression_(expression),
      sf_(std::make_shared<SourceFactory>(expression)) {
  for (const auto& m : macros) {
    macros_.emplace(m.macroKey(), m);
  }
}

ParserVisitor::~ParserVisitor() {}

template <typename T, typename = std::enable_if_t<
                          std::is_base_of<antlr4::tree::ParseTree, T>::value>>
T* tree_as(antlr4::tree::ParseTree* tree) {
  return dynamic_cast<T*>(tree);
}

antlrcpp::Any ParserVisitor::visit(antlr4::tree::ParseTree* tree) {
  if (auto* ctx = tree_as<CelParser::StartContext>(tree)) {
    return visitStart(ctx);
  } else if (auto* ctx = tree_as<CelParser::ExprContext>(tree)) {
    return visitExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConditionalAndContext>(tree)) {
    return visitConditionalAnd(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConditionalOrContext>(tree)) {
    return visitConditionalOr(ctx);
  } else if (auto* ctx = tree_as<CelParser::RelationContext>(tree)) {
    return visitRelation(ctx);
  } else if (auto* ctx = tree_as<CelParser::CalcContext>(tree)) {
    return visitCalc(ctx);
  } else if (auto* ctx = tree_as<CelParser::LogicalNotContext>(tree)) {
    return visitLogicalNot(ctx);
  } else if (auto* ctx = tree_as<CelParser::PrimaryExprContext>(tree)) {
    return visitPrimaryExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::MemberExprContext>(tree)) {
    return visitMemberExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::SelectOrCallContext>(tree)) {
    return visitSelectOrCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::MapInitializerListContext>(tree)) {
    return visitMapInitializerList(ctx);
  } else if (auto* ctx = tree_as<CelParser::NegateContext>(tree)) {
    return visitNegate(ctx);
  } else if (auto* ctx = tree_as<CelParser::IndexContext>(tree)) {
    return visitIndex(ctx);
  } else if (auto* ctx = tree_as<CelParser::UnaryContext>(tree)) {
    return visitUnary(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateListContext>(tree)) {
    return visitCreateList(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateMessageContext>(tree)) {
    return visitCreateMessage(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateStructContext>(tree)) {
    return visitCreateStruct(ctx);
  }

  std::string text = "<<nil>>";
  if (tree) {
    text = tree->getText();
  }
  return sf_->reportError(tree_as<antlr4::ParserRuleContext>(tree),
                          "unknown parsetree type");
}

antlrcpp::Any ParserVisitor::visitPrimaryExpr(
    CelParser::PrimaryExprContext* pctx) {
  CelParser::PrimaryContext* primary = pctx->primary();
  if (auto* ctx = tree_as<CelParser::NestedContext>(primary)) {
    return visitNested(ctx);
  } else if (auto* ctx =
                 tree_as<CelParser::IdentOrGlobalCallContext>(primary)) {
    return visitIdentOrGlobalCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateListContext>(primary)) {
    return visitCreateList(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateStructContext>(primary)) {
    return visitCreateStruct(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConstantLiteralContext>(primary)) {
    return visitConstantLiteral(ctx);
  }
  return sf_->reportError(pctx, "invalid primary expression");
}

antlrcpp::Any ParserVisitor::visitMemberExpr(
    CelParser::MemberExprContext* mctx) {
  CelParser::MemberContext* member = mctx->member();
  if (auto* ctx = tree_as<CelParser::PrimaryExprContext>(member)) {
    return visitPrimaryExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::SelectOrCallContext>(member)) {
    return visitSelectOrCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::IndexContext>(member)) {
    return visitIndex(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateMessageContext>(member)) {
    return visitCreateMessage(ctx);
  }
  return sf_->reportError(mctx, "unsupported simple expression");
}

antlrcpp::Any ParserVisitor::visitStart(CelParser::StartContext* ctx) {
  return visit(ctx->expr());
}

antlrcpp::Any ParserVisitor::visitExpr(CelParser::ExprContext* ctx) {
  auto result = visit(ctx->e);
  if (!ctx->op) {
    return result;
  }
  int64_t op_id = sf_->id(ctx->op);
  Expr if_true = visit(ctx->e1);
  Expr if_false = visit(ctx->e2);

  return globalCallOrMacro(op_id, CelOperator::CONDITIONAL,
                           {result, if_true, if_false});
}

antlrcpp::Any ParserVisitor::visitConditionalOr(
    CelParser::ConditionalOrContext* ctx) {
  auto result = visit(ctx->e);
  if (ctx->ops.empty()) {
    return result;
  }
  ExpressionBalancer b(sf_, CelOperator::LOGICAL_OR, result);
  for (size_t i = 0; i < ctx->ops.size(); ++i) {
    auto op = ctx->ops[i];
    auto next = visit(ctx->e1[i]).as<Expr>();
    int64_t op_id = sf_->id(op);
    b.addTerm(op_id, next);
  }
  return b.balance();
}

antlrcpp::Any ParserVisitor::visitConditionalAnd(
    CelParser::ConditionalAndContext* ctx) {
  auto result = visit(ctx->e);
  if (ctx->ops.empty()) {
    return result;
  }
  ExpressionBalancer b(sf_, CelOperator::LOGICAL_AND, result);
  for (size_t i = 0; i < ctx->ops.size(); ++i) {
    auto op = ctx->ops[i];
    auto next = visit(ctx->e1[i]).as<Expr>();
    int64_t op_id = sf_->id(op);
    b.addTerm(op_id, next);
  }
  return b.balance();
}

antlrcpp::Any ParserVisitor::visitRelation(CelParser::RelationContext* ctx) {
  if (ctx->calc()) {
    return visit(ctx->calc());
  }
  std::string op_text;
  if (ctx->op) {
    op_text = ctx->op->getText();
  }
  auto op = ReverseLookupOperator(op_text);
  if (op) {
    auto lhs = visit(ctx->relation(0)).as<Expr>();
    int64_t op_id = sf_->id(ctx->op);
    auto rhs = visit(ctx->relation(1)).as<Expr>();
    return globalCallOrMacro(op_id, *op, {lhs, rhs});
  }
  return sf_->reportError(ctx, "operator not found");
}

antlrcpp::Any ParserVisitor::visitCalc(CelParser::CalcContext* ctx) {
  if (ctx->unary()) {
    return visit(ctx->unary());
  }
  std::string op_text;
  if (ctx->op) {
    op_text = ctx->op->getText();
  }
  auto op = ReverseLookupOperator(op_text);
  if (op) {
    auto lhs = visit(ctx->calc(0)).as<Expr>();
    int64_t op_id = sf_->id(ctx->op);
    auto rhs = visit(ctx->calc(1)).as<Expr>();
    return globalCallOrMacro(op_id, *op, {lhs, rhs});
  }
  return sf_->reportError(ctx, "operator not found");
}

antlrcpp::Any ParserVisitor::visitUnary(CelParser::UnaryContext* ctx) {
  return sf_->newLiteralString(ctx, "<<error>>");
}

antlrcpp::Any ParserVisitor::visitLogicalNot(
    CelParser::LogicalNotContext* ctx) {
  if (ctx->ops.size() % 2 == 0) {
    return visit(ctx->member());
  }
  int64_t op_id = sf_->id(ctx->ops[0]);
  auto target = visit(ctx->member());
  return globalCallOrMacro(op_id, CelOperator::LOGICAL_NOT, {target});
}

antlrcpp::Any ParserVisitor::visitNegate(CelParser::NegateContext* ctx) {
  if (ctx->ops.size() % 2 == 0) {
    return visit(ctx->member());
  }
  int64_t op_id = sf_->id(ctx->ops[0]);
  auto target = visit(ctx->member());
  return globalCallOrMacro(op_id, CelOperator::NEGATE, {target});
}

antlrcpp::Any ParserVisitor::visitSelectOrCall(
    CelParser::SelectOrCallContext* ctx) {
  auto operand = visit(ctx->member()).as<Expr>();
  // Handle the error case where no valid identifier is specified.
  if (!ctx->id) {
    return sf_->newExpr(ctx);
  }
  auto id = ctx->id->getText();
  if (ctx->open) {
    int64_t op_id = sf_->id(ctx->open);
    return receiverCallOrMacro(op_id, id, operand, visitList(ctx->args));
  }
  return sf_->newSelect(ctx, operand, id);
}

antlrcpp::Any ParserVisitor::visitIndex(CelParser::IndexContext* ctx) {
  auto target = visit(ctx->member()).as<Expr>();
  int64_t op_id = sf_->id(ctx->op);
  auto index = visit(ctx->index).as<Expr>();
  return globalCallOrMacro(op_id, CelOperator::INDEX, {target, index});
}

antlrcpp::Any ParserVisitor::visitCreateMessage(
    CelParser::CreateMessageContext* ctx) {
  auto target = visit(ctx->member()).as<Expr>();
  int64_t obj_id = sf_->id(ctx->op);
  std::string message_name = extractQualifiedName(ctx, &target);
  if (!message_name.empty()) {
    auto entries = visitFieldInitializerList(ctx->entries)
                       .as<std::vector<Expr::CreateStruct::Entry>>();
    return sf_->newObject(obj_id, message_name, entries);
  } else {
    return sf_->newExpr(obj_id);
  }
}

antlrcpp::Any ParserVisitor::visitFieldInitializerList(
    CelParser::FieldInitializerListContext* ctx) {
  std::vector<Expr::CreateStruct::Entry> res;
  if (!ctx || ctx->fields.empty()) {
    return res;
  }

  res.resize(ctx->fields.size());
  for (size_t i = 0; i < ctx->fields.size(); ++i) {
    const auto& f = ctx->fields[i];
    int64_t init_id = sf_->id(ctx->cols[i]);
    auto value = visit(ctx->values[i]).as<Expr>();
    auto field = sf_->newObjectField(init_id, f->getText(), value);
    res[i] = field;
  }

  return res;
}

antlrcpp::Any ParserVisitor::visitIdentOrGlobalCall(
    CelParser::IdentOrGlobalCallContext* ctx) {
  std::string ident_name;
  if (ctx->leadingDot) {
    ident_name = ".";
  }
  if (!ctx->id) {
    return sf_->newExpr(ctx);
  }
  if (sf_->isReserved(ctx->id->getText())) {
    return sf_->reportError(
        ctx, absl::StrFormat("reserved identifier: %s", ctx->id->getText()));
  }
  // check if ID is in reserved identifiers
  ident_name += ctx->id->getText();
  if (ctx->op) {
    int64_t op_id = sf_->id(ctx->op);
    return globalCallOrMacro(op_id, ident_name, visitList(ctx->args));
  }
  return sf_->newIdent(ctx->id, ident_name);
}

antlrcpp::Any ParserVisitor::visitNested(CelParser::NestedContext* ctx) {
  return visit(ctx->e);
}

antlrcpp::Any ParserVisitor::visitCreateList(
    CelParser::CreateListContext* ctx) {
  int64_t list_id = sf_->id(ctx->op);
  return sf_->newList(list_id, visitList(ctx->elems));
}

std::vector<Expr> ParserVisitor::visitList(CelParser::ExprListContext* ctx) {
  std::vector<Expr> rv;
  if (!ctx) return rv;
  std::transform(ctx->e.begin(), ctx->e.end(), std::back_inserter(rv),
                 [this](CelParser::ExprContext* expr_ctx) {
                   return visitExpr(expr_ctx).as<Expr>();
                 });
  return rv;
}

antlrcpp::Any ParserVisitor::visitCreateStruct(
    CelParser::CreateStructContext* ctx) {
  int64_t struct_id = sf_->id(ctx->op);
  std::vector<Expr::CreateStruct::Entry> entries;
  if (ctx->entries) {
    entries = visitMapInitializerList(ctx->entries)
                  .as<std::vector<Expr::CreateStruct::Entry>>();
  }
  return sf_->newMap(struct_id, entries);
}

antlrcpp::Any ParserVisitor::visitMapInitializerList(
    CelParser::MapInitializerListContext* ctx) {
  std::vector<Expr::CreateStruct::Entry> res;
  if (!ctx || ctx->keys.empty()) {
    return res;
  }

  res.resize(ctx->cols.size());
  for (size_t i = 0; i < ctx->cols.size(); ++i) {
    int64_t col_id = sf_->id(ctx->cols[i]);
    auto key = visit(ctx->keys[i]);
    auto value = visit(ctx->values[i]);
    res[i] = sf_->newMapEntry(col_id, key, value);
  }
  return res;
}

antlrcpp::Any ParserVisitor::visitInt(CelParser::IntContext* ctx) {
  std::string value;
  if (ctx->sign) {
    value = ctx->sign->getText();
  }
  value += ctx->tok->getText();
  int64_t int_value;
  if (absl::SimpleAtoi(value, &int_value)) {
    return sf_->newLiteralInt(ctx, int_value);
  } else {
    return sf_->reportError(ctx, "invalid int literal");
  }
}

antlrcpp::Any ParserVisitor::visitUint(CelParser::UintContext* ctx) {
  std::string value = ctx->tok->getText();
  // trim the 'u' designator included in the uint literal.
  if (!value.empty()) {
    value.resize(value.size() - 1);
  }
  uint64_t uint_value;
  if (absl::SimpleAtoi(value, &uint_value)) {
    return sf_->newLiteralUint(ctx, uint_value);
  } else {
    return sf_->reportError(ctx, "invalid uint literal");
  }
}

antlrcpp::Any ParserVisitor::visitDouble(CelParser::DoubleContext* ctx) {
  std::string value;
  if (ctx->sign) {
    value = ctx->sign->getText();
  }
  value += ctx->tok->getText();
  double double_value;
  if (absl::SimpleAtod(value, &double_value)) {
    return sf_->newLiteralDouble(ctx, double_value);
  } else {
    return sf_->reportError(ctx, "invalid double literal");
  }
}

antlrcpp::Any ParserVisitor::visitString(CelParser::StringContext* ctx) {
  std::string value = unquote(ctx, ctx->tok->getText(), /* is bytes */ false);
  return sf_->newLiteralString(ctx, value);
}

antlrcpp::Any ParserVisitor::visitBytes(CelParser::BytesContext* ctx) {
  std::string value = unquote(ctx, ctx->tok->getText().substr(1),
                              /* is bytes */ true);
  return sf_->newLiteralBytes(ctx, value);
}

antlrcpp::Any ParserVisitor::visitBoolTrue(CelParser::BoolTrueContext* ctx) {
  return sf_->newLiteralBool(ctx, true);
}

antlrcpp::Any ParserVisitor::visitBoolFalse(CelParser::BoolFalseContext* ctx) {
  return sf_->newLiteralBool(ctx, false);
}

antlrcpp::Any ParserVisitor::visitNull(CelParser::NullContext* ctx) {
  return sf_->newLiteralNull(ctx);
}

google::api::expr::v1alpha1::SourceInfo ParserVisitor::sourceInfo() const {
  return sf_->sourceInfo();
}

EnrichedSourceInfo ParserVisitor::enrichedSourceInfo() const {
  return sf_->enrichedSourceInfo();
}

void ParserVisitor::syntaxError(antlr4::Recognizer* recognizer,
                                antlr4::Token* offending_symbol, size_t line,
                                size_t col, const std::string& msg,
                                std::exception_ptr e) {
  sf_->reportError(line, col, "Syntax error: " + msg);
}

bool ParserVisitor::hasErrored() const { return !sf_->errors().empty(); }

absl::optional<int32_t> ParserVisitor::findLineOffset(int32_t line) const {
  // note that err.line is 1-based,
  // while we need the 0-based index
  const std::vector<int32_t>& line_offsets = sf_->line_offsets();
  if (line == 1) {
    return 0;
  } else if (line > 1 && line <= static_cast<int32_t>(line_offsets.size())) {
    return line_offsets[line - 2];
  }
  return {};
}

std::string ParserVisitor::getSourceLine(int32_t line) const {
  auto char_start = findLineOffset(line);
  if (!char_start) {
    return "";
  }
  auto char_end = findLineOffset(line + 1);
  if (char_end) {
    return expression_.substr(*char_start, *char_end - *char_end - 1);
  } else {
    return expression_.substr(*char_start);
  }
}

std::string ParserVisitor::errorMessage() const {
  std::vector<std::string> messages;
  std::transform(
      sf_->errors().begin(), sf_->errors().end(), std::back_inserter(messages),
      [this](const SourceFactory::Error& error) {
        std::string s = absl::StrFormat("ERROR: %s:%zu:%zu: %s", description_,
                                        error.location.line,
                                        // add one to the 0-based column
                                        error.location.col + 1, error.message);
        std::string snippet = getSourceLine(error.location.line);
        std::string::size_type pos = 0;
        while ((pos = snippet.find("\t", pos)) != std::string::npos) {
          snippet.replace(pos, 1, " ");
        }
        std::string src_line = "\n | " + snippet;
        std::string ind_line = "\n | ";
        for (int i = 0; i < error.location.col; ++i) {
          ind_line += ".";
        }
        ind_line += "^";
        s += src_line + ind_line;
        return s;
      });
  return absl::StrJoin(messages, "\n");
}

Expr ParserVisitor::globalCallOrMacro(int64_t expr_id, std::string function,
                                      std::vector<Expr> args) {
  Expr macro_expr;
  if (expandMacro(expr_id, function, nullptr, args, &macro_expr)) {
    return macro_expr;
  }

  return sf_->newGlobalCall(expr_id, function, args);
}

Expr ParserVisitor::receiverCallOrMacro(int64_t expr_id, std::string function,
                                        Expr target, std::vector<Expr> args) {
  Expr macro_expr;
  if (expandMacro(expr_id, function, &target, args, &macro_expr)) {
    return macro_expr;
  }

  return sf_->newReceiverCall(expr_id, function, target, args);
}

bool ParserVisitor::expandMacro(int64_t expr_id, std::string function,
                                Expr* target, std::vector<Expr> args,
                                Expr* macro_expr) {
  std::string macro_key = absl::StrFormat("%s:%d:%s", function, args.size(),
                                          target ? "true" : "false");
  auto m = macros_.find(macro_key);
  if (m == macros_.end()) {
    std::string var_arg_macro_key =
        absl::StrFormat("%s:*:%s", function, target ? "true" : "false");
    m = macros_.find(var_arg_macro_key);
    if (m == macros_.end()) {
      return false;
    }
  }

  Expr expr = m->second.expand(sf_, expr_id, target, args);
  if (expr.expr_kind_case() != Expr::EXPR_KIND_NOT_SET) {
    macro_expr->CopyFrom(expr);
    return true;
  }
  return false;
}

std::string ParserVisitor::unquote(antlr4::ParserRuleContext* ctx,
                                   const std::string& s, bool is_bytes) {
  auto text = unescape(s, is_bytes);
  if (!text) {
    sf_->reportError(ctx, "failed to unquote");
    return s;
  }
  return *text;
}

std::string ParserVisitor::extractQualifiedName(antlr4::ParserRuleContext* ctx,
                                                const Expr* e) {
  if (!e) {
    return "";
  }

  switch (e->expr_kind_case()) {
    case Expr::kIdentExpr:
      return e->ident_expr().name();
    case Expr::kSelectExpr: {
      auto& s = e->select_expr();
      std::string prefix = extractQualifiedName(ctx, &s.operand());
      if (!prefix.empty()) {
        return prefix + "." + s.field();
      }
    } break;
    default:
      break;
  }
  sf_->reportError(sf_->getSourceLocation(e->id()),
                   "expected a qualified name");
  return "";
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
