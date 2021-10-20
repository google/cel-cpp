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

#include "parser/parser.h"

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/types/optional.h"
#include "common/escaping.h"
#include "common/operators.h"
#include "parser/internal/cel_grammar.inc/cel_parser_internal/CelBaseVisitor.h"
#include "parser/internal/cel_grammar.inc/cel_parser_internal/CelLexer.h"
#include "parser/internal/cel_grammar.inc/cel_parser_internal/CelParser.h"
#include "parser/macro.h"
#include "parser/options.h"
#include "parser/source_factory.h"
#include "antlr4-runtime.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

namespace {

using ::antlr4::ANTLRInputStream;
using ::antlr4::CommonTokenStream;
using ::antlr4::DefaultErrorStrategy;
using ::antlr4::ParseCancellationException;
using ::antlr4::Parser;
using ::antlr4::ParserRuleContext;
using ::antlr4::Token;
using ::antlr4::misc::IntervalSet;
using ::antlr4::tree::ErrorNode;
using ::antlr4::tree::ParseTreeListener;
using ::antlr4::tree::TerminalNode;
using ::cel::parser_internal::CelBaseVisitor;
using ::cel::parser_internal::CelLexer;
using ::cel::parser_internal::CelParser;
using common::CelOperator;
using common::ReverseLookupOperator;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;

// Scoped helper for incrementing the parse recursion count.
// Increments on creation, decrements on destruction (stack unwind).
class ScopedIncrement final {
 public:
  explicit ScopedIncrement(int& recursion_depth)
      : recursion_depth_(recursion_depth) {
    ++recursion_depth_;
  }

  ~ScopedIncrement() { --recursion_depth_; }

 private:
  int& recursion_depth_;
};

// balancer performs tree balancing on operators whose arguments are of equal
// precedence.
//
// The purpose of the balancer is to ensure a compact serialization format for
// the logical &&, || operators which have a tendency to create long DAGs which
// are skewed in one direction. Since the operators are commutative re-ordering
// the terms *must not* affect the evaluation result.
//
// Based on code from //third_party/cel/go/parser/helper.go
class ExpressionBalancer final {
 public:
  ExpressionBalancer(std::shared_ptr<SourceFactory> sf, std::string function,
                     Expr expr);

  // addTerm adds an operation identifier and term to the set of terms to be
  // balanced.
  void addTerm(int64_t op, Expr term);

  // balance creates a balanced tree from the sub-terms and returns the final
  // Expr value.
  Expr balance();

 private:
  // balancedTree recursively balances the terms provided to a commutative
  // operator.
  Expr balancedTree(int lo, int hi);

 private:
  std::shared_ptr<SourceFactory> sf_;
  std::string function_;
  std::vector<Expr> terms_;
  std::vector<int64_t> ops_;
};

ExpressionBalancer::ExpressionBalancer(std::shared_ptr<SourceFactory> sf,
                                       std::string function, Expr expr)
    : sf_(std::move(sf)),
      function_(std::move(function)),
      terms_{std::move(expr)},
      ops_{} {}

void ExpressionBalancer::addTerm(int64_t op, Expr term) {
  terms_.push_back(std::move(term));
  ops_.push_back(op);
}

Expr ExpressionBalancer::balance() {
  if (terms_.size() == 1) {
    return terms_[0];
  }
  return balancedTree(0, ops_.size() - 1);
}

Expr ExpressionBalancer::balancedTree(int lo, int hi) {
  int mid = (lo + hi + 1) / 2;

  Expr left;
  if (mid == lo) {
    left = terms_[mid];
  } else {
    left = balancedTree(lo, mid - 1);
  }

  Expr right;
  if (mid == hi) {
    right = terms_[mid + 1];
  } else {
    right = balancedTree(mid + 1, hi);
  }
  return sf_->newGlobalCall(ops_[mid], function_,
                            {std::move(left), std::move(right)});
}

class ParserVisitor final : public CelBaseVisitor,
                            public antlr4::BaseErrorListener {
 public:
  ParserVisitor(const std::string& description, const std::string& expression,
                const int max_recursion_depth,
                const std::vector<Macro>& macros = {},
                const bool add_macro_calls = false);
  ~ParserVisitor() override;

  antlrcpp::Any visit(antlr4::tree::ParseTree* tree) override;

  antlrcpp::Any visitStart(CelParser::StartContext* ctx) override;
  antlrcpp::Any visitExpr(CelParser::ExprContext* ctx) override;
  antlrcpp::Any visitConditionalOr(
      CelParser::ConditionalOrContext* ctx) override;
  antlrcpp::Any visitConditionalAnd(
      CelParser::ConditionalAndContext* ctx) override;
  antlrcpp::Any visitRelation(CelParser::RelationContext* ctx) override;
  antlrcpp::Any visitCalc(CelParser::CalcContext* ctx) override;
  antlrcpp::Any visitUnary(CelParser::UnaryContext* ctx);
  antlrcpp::Any visitLogicalNot(CelParser::LogicalNotContext* ctx) override;
  antlrcpp::Any visitNegate(CelParser::NegateContext* ctx) override;
  antlrcpp::Any visitSelectOrCall(CelParser::SelectOrCallContext* ctx) override;
  antlrcpp::Any visitIndex(CelParser::IndexContext* ctx) override;
  antlrcpp::Any visitCreateMessage(
      CelParser::CreateMessageContext* ctx) override;
  antlrcpp::Any visitFieldInitializerList(
      CelParser::FieldInitializerListContext* ctx) override;
  antlrcpp::Any visitIdentOrGlobalCall(
      CelParser::IdentOrGlobalCallContext* ctx) override;
  antlrcpp::Any visitNested(CelParser::NestedContext* ctx) override;
  antlrcpp::Any visitCreateList(CelParser::CreateListContext* ctx) override;
  std::vector<google::api::expr::v1alpha1::Expr> visitList(
      CelParser::ExprListContext* ctx);
  antlrcpp::Any visitCreateStruct(CelParser::CreateStructContext* ctx) override;
  antlrcpp::Any visitConstantLiteral(
      CelParser::ConstantLiteralContext* ctx) override;
  antlrcpp::Any visitPrimaryExpr(CelParser::PrimaryExprContext* ctx) override;
  antlrcpp::Any visitMemberExpr(CelParser::MemberExprContext* ctx) override;

  antlrcpp::Any visitMapInitializerList(
      CelParser::MapInitializerListContext* ctx) override;
  antlrcpp::Any visitInt(CelParser::IntContext* ctx) override;
  antlrcpp::Any visitUint(CelParser::UintContext* ctx) override;
  antlrcpp::Any visitDouble(CelParser::DoubleContext* ctx) override;
  antlrcpp::Any visitString(CelParser::StringContext* ctx) override;
  antlrcpp::Any visitBytes(CelParser::BytesContext* ctx) override;
  antlrcpp::Any visitBoolTrue(CelParser::BoolTrueContext* ctx) override;
  antlrcpp::Any visitBoolFalse(CelParser::BoolFalseContext* ctx) override;
  antlrcpp::Any visitNull(CelParser::NullContext* ctx) override;
  google::api::expr::v1alpha1::SourceInfo sourceInfo() const;
  EnrichedSourceInfo enrichedSourceInfo() const;
  void syntaxError(antlr4::Recognizer* recognizer,
                   antlr4::Token* offending_symbol, size_t line, size_t col,
                   const std::string& msg, std::exception_ptr e) override;
  bool hasErrored() const;

  std::string errorMessage() const;

 private:
  Expr globalCallOrMacro(int64_t expr_id, const std::string& function,
                         const std::vector<Expr>& args);
  Expr receiverCallOrMacro(int64_t expr_id, const std::string& function,
                           const Expr& target, const std::vector<Expr>& args);
  bool expandMacro(int64_t expr_id, const std::string& function,
                   const Expr& target, const std::vector<Expr>& args,
                   Expr* macro_expr);
  std::string unquote(antlr4::ParserRuleContext* ctx, const std::string& s,
                      bool is_bytes);
  std::string extractQualifiedName(antlr4::ParserRuleContext* ctx,
                                   const Expr* e);

 private:
  std::string description_;
  std::string expression_;
  std::shared_ptr<SourceFactory> sf_;
  std::map<std::string, Macro> macros_;
  int recursion_depth_;
  const int max_recursion_depth_;
  const bool add_macro_calls_;
};

ParserVisitor::ParserVisitor(const std::string& description,
                             const std::string& expression,
                             const int max_recursion_depth,
                             const std::vector<Macro>& macros,
                             const bool add_macro_calls)
    : description_(description),
      expression_(expression),
      sf_(std::make_shared<SourceFactory>(expression)),
      recursion_depth_(0),
      max_recursion_depth_(max_recursion_depth),
      add_macro_calls_(add_macro_calls) {
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
  ScopedIncrement inc(recursion_depth_);
  if (recursion_depth_ > max_recursion_depth_) {
    return sf_->reportError(
        SourceFactory::noLocation(),
        absl::StrFormat("Exceeded max recursion depth of %d when parsing.",
                        max_recursion_depth_));
  }
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

  if (tree) {
    return sf_->reportError(tree_as<antlr4::ParserRuleContext>(tree),
                            "unknown parsetree type");
  }
  return sf_->reportError(SourceFactory::noLocation(), "<<nil>> parsetree");
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
    if (i >= ctx->e1.size()) {
      return sf_->reportError(ctx, "unexpected character, wanted '||'");
    }
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
    if (i >= ctx->e1.size()) {
      return sf_->reportError(ctx, "unexpected character, wanted '&&'");
    }
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
    if (i >= ctx->cols.size() || i >= ctx->values.size()) {
      // This is the result of a syntax error detected elsewhere.
      return res;
    }
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

antlrcpp::Any ParserVisitor::visitConstantLiteral(
    CelParser::ConstantLiteralContext* clctx) {
  CelParser::LiteralContext* literal = clctx->literal();
  if (auto* ctx = tree_as<CelParser::IntContext>(literal)) {
    return visitInt(ctx);
  } else if (auto* ctx = tree_as<CelParser::UintContext>(literal)) {
    return visitUint(ctx);
  } else if (auto* ctx = tree_as<CelParser::DoubleContext>(literal)) {
    return visitDouble(ctx);
  } else if (auto* ctx = tree_as<CelParser::StringContext>(literal)) {
    return visitString(ctx);
  } else if (auto* ctx = tree_as<CelParser::BytesContext>(literal)) {
    return visitBytes(ctx);
  } else if (auto* ctx = tree_as<CelParser::BoolFalseContext>(literal)) {
    return visitBoolFalse(ctx);
  } else if (auto* ctx = tree_as<CelParser::BoolTrueContext>(literal)) {
    return visitBoolTrue(ctx);
  } else if (auto* ctx = tree_as<CelParser::NullContext>(literal)) {
    return visitNull(ctx);
  }
  return sf_->reportError(clctx, "invalid constant literal expression");
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
  int base = 10;
  if (absl::StartsWith(ctx->tok->getText(), "0x")) {
    base = 16;
  }
  value += ctx->tok->getText();
  int64_t int_value;
  if (absl::numbers_internal::safe_strto64_base(value, &int_value, base)) {
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
  int base = 10;
  if (absl::StartsWith(ctx->tok->getText(), "0x")) {
    base = 16;
  }
  uint64_t uint_value;
  if (absl::numbers_internal::safe_strtou64_base(value, &uint_value, base)) {
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

std::string ParserVisitor::errorMessage() const {
  return sf_->errorMessage(description_, expression_);
}

Expr ParserVisitor::globalCallOrMacro(int64_t expr_id,
                                      const std::string& function,
                                      const std::vector<Expr>& args) {
  Expr macro_expr;
  if (expandMacro(expr_id, function, Expr::default_instance(), args,
                  &macro_expr)) {
    return macro_expr;
  }

  return sf_->newGlobalCall(expr_id, function, args);
}

Expr ParserVisitor::receiverCallOrMacro(int64_t expr_id,
                                        const std::string& function,
                                        const Expr& target,
                                        const std::vector<Expr>& args) {
  Expr macro_expr;
  if (expandMacro(expr_id, function, target, args, &macro_expr)) {
    return macro_expr;
  }

  return sf_->newReceiverCall(expr_id, function, target, args);
}

bool ParserVisitor::expandMacro(int64_t expr_id, const std::string& function,
                                const Expr& target,
                                const std::vector<Expr>& args,
                                Expr* macro_expr) {
  std::string macro_key = absl::StrFormat("%s:%d:%s", function, args.size(),
                                          target.id() != 0 ? "true" : "false");
  auto m = macros_.find(macro_key);
  if (m == macros_.end()) {
    std::string var_arg_macro_key = absl::StrFormat(
        "%s:*:%s", function, target.id() != 0 ? "true" : "false");
    m = macros_.find(var_arg_macro_key);
    if (m == macros_.end()) {
      return false;
    }
  }

  Expr expr = m->second.expand(sf_, expr_id, target, args);
  if (expr.expr_kind_case() != Expr::EXPR_KIND_NOT_SET) {
    *macro_expr = std::move(expr);
    if (add_macro_calls_) {
      // If the macro is nested, the full expression id is used as an argument
      // id in the tree. Using this ID instead of expr_id allows argument id
      // lookups in macro_calls when building the map and iterating
      // the AST.
      sf_->AddMacroCall(macro_expr->id(), target, args, function);
    }
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

// Replacements for absl::StrReplaceAll for escaping standard whitespace
// characters.
static constexpr auto kStandardReplacements =
    std::array<std::pair<absl::string_view, absl::string_view>, 3>{
        std::make_pair("\n", "\\n"),
        std::make_pair("\r", "\\r"),
        std::make_pair("\t", "\\t"),
    };

static constexpr absl::string_view kSingleQuote = "'";

// ExprRecursionListener extends the standard ANTLR CelParser to ensure that
// recursive entries into the 'expr' rule are limited to a configurable depth so
// as to prevent stack overflows.
class ExprRecursionListener : public ParseTreeListener {
 public:
  explicit ExprRecursionListener(
      const int max_recursion_depth = kDefaultMaxRecursionDepth)
      : max_recursion_depth_(max_recursion_depth), recursion_depth_(0) {}
  ~ExprRecursionListener() override {}

  void visitTerminal(TerminalNode* node) override{};
  void visitErrorNode(ErrorNode* error) override{};
  void enterEveryRule(ParserRuleContext* ctx) override;
  void exitEveryRule(ParserRuleContext* ctx) override;

 private:
  const int max_recursion_depth_;
  int recursion_depth_;
};

void ExprRecursionListener::enterEveryRule(ParserRuleContext* ctx) {
  // Throw a ParseCancellationException since the parsing would otherwise
  // continue if this were treated as a syntax error and the problem would
  // continue to manifest.
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    if (recursion_depth_ >= max_recursion_depth_) {
      throw ParseCancellationException(
          absl::StrFormat("Expression recursion limit exceeded. limit: %d",
                          max_recursion_depth_));
    }
    recursion_depth_++;
  }
}

void ExprRecursionListener::exitEveryRule(ParserRuleContext* ctx) {
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    recursion_depth_--;
  }
}

class RecoveryLimitErrorStrategy : public DefaultErrorStrategy {
 public:
  explicit RecoveryLimitErrorStrategy(
      int recovery_limit = kDefaultErrorRecoveryLimit,
      int recovery_token_lookahead_limit =
          kDefaultErrorRecoveryTokenLookaheadLimit)
      : recovery_limit_(recovery_limit),
        recovery_attempts_(0),
        recovery_token_lookahead_limit_(recovery_token_lookahead_limit) {}

  void recover(Parser* recognizer, std::exception_ptr e) override {
    checkRecoveryLimit(recognizer);
    DefaultErrorStrategy::recover(recognizer, e);
  }

  Token* recoverInline(Parser* recognizer) override {
    checkRecoveryLimit(recognizer);
    return DefaultErrorStrategy::recoverInline(recognizer);
  }

  // Override the ANTLR implementation to introduce a token lookahead limit as
  // this prevents pathologically constructed, yet small (< 16kb) inputs from
  // consuming inordinate amounts of compute.
  //
  // This method is only called on error recovery paths.
  void consumeUntil(Parser* recognizer, const IntervalSet& set) override {
    size_t ttype = recognizer->getInputStream()->LA(1);
    int recovery_search_depth = 0;
    while (ttype != Token::EOF && !set.contains(ttype) &&
           recovery_search_depth++ < recovery_token_lookahead_limit_) {
      recognizer->consume();
      ttype = recognizer->getInputStream()->LA(1);
    }
    // Halt all parsing if the lookahead limit is reached during error recovery.
    if (recovery_search_depth == recovery_token_lookahead_limit_) {
      throw ParseCancellationException("Unable to find a recovery token");
    }
  }

 protected:
  std::string escapeWSAndQuote(const std::string& s) const override {
    std::string result;
    result.reserve(s.size() + 2);
    absl::StrAppend(&result, kSingleQuote, s, kSingleQuote);
    absl::StrReplaceAll(kStandardReplacements, &result);
    return result;
  }

 private:
  void checkRecoveryLimit(Parser* recognizer) {
    if (recovery_attempts_++ >= recovery_limit_) {
      std::string too_many_errors =
          absl::StrFormat("More than %d parse errors.", recovery_limit_);
      recognizer->notifyErrorListeners(too_many_errors);
      throw ParseCancellationException(too_many_errors);
    }
  }

  int recovery_limit_;
  int recovery_attempts_;
  int recovery_token_lookahead_limit_;
};

}  // namespace

absl::StatusOr<ParsedExpr> Parse(const std::string& expression,
                                 const std::string& description,
                                 const ParserOptions& options) {
  return ParseWithMacros(expression, Macro::AllMacros(), description, options);
}

absl::StatusOr<ParsedExpr> ParseWithMacros(const std::string& expression,
                                           const std::vector<Macro>& macros,
                                           const std::string& description,
                                           const ParserOptions& options) {
  auto result = EnrichedParse(expression, macros, description, options);
  if (result.ok()) {
    return result->parsed_expr();
  }
  return result.status();
}

absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description, const ParserOptions& options) {
  ANTLRInputStream input(expression);
  if (input.size() > options.expression_size_codepoint_limit) {
    return absl::InvalidArgumentError(absl::StrCat(
        "expression size exceeds codepoint limit.", " input size: ",
        input.size(), ", limit: ", options.expression_size_codepoint_limit));
  }
  CelLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  CelParser parser(&tokens);
  ExprRecursionListener listener(options.max_recursion_depth);
  ParserVisitor visitor(description, expression, options.max_recursion_depth,
                        macros, options.add_macro_calls);

  lexer.removeErrorListeners();
  parser.removeErrorListeners();
  lexer.addErrorListener(&visitor);
  parser.addErrorListener(&visitor);
  parser.addParseListener(&listener);

  // Limit the number of error recovery attempts to prevent bad expressions
  // from consuming lots of cpu / memory.
  std::shared_ptr<RecoveryLimitErrorStrategy> error_strategy(
      new RecoveryLimitErrorStrategy(
          options.error_recovery_limit,
          options.error_recovery_token_lookahead_limit));
  parser.setErrorHandler(error_strategy);

  CelParser::StartContext* root;
  try {
    root = parser.start();
  } catch (const ParseCancellationException& e) {
    if (visitor.hasErrored()) {
      return absl::InvalidArgumentError(visitor.errorMessage());
    }
    return absl::CancelledError(e.what());
  } catch (const std::exception& e) {
    return absl::AbortedError(e.what());
  }

  Expr expr = visitor.visit(root).as<Expr>();
  if (visitor.hasErrored()) {
    return absl::InvalidArgumentError(visitor.errorMessage());
  }

  // root is deleted as part of the parser context
  ParsedExpr parsed_expr;
  *(parsed_expr.mutable_expr()) = std::move(expr);
  auto enriched_source_info = visitor.enrichedSourceInfo();
  *(parsed_expr.mutable_source_info()) = visitor.sourceInfo();
  return VerboseParsedExpr(std::move(parsed_expr),
                           std::move(enriched_source_info));
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
