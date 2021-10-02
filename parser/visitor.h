#ifndef THIRD_PARTY_CEL_CPP_PARSER_VISITOR_H_
#define THIRD_PARTY_CEL_CPP_PARSER_VISITOR_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/types/optional.h"
#include "parser/cel_grammar.inc/cel_grammar/CelBaseVisitor.h"
#include "parser/macro.h"
#include "parser/source_factory.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

class SourceFactory;

class ParserVisitor : public ::cel_grammar::CelBaseVisitor,
                      public antlr4::BaseErrorListener {
 public:
  ParserVisitor(const std::string& description, const std::string& expression,
                const int max_recursion_depth,
                const std::vector<Macro>& macros = {},
                const bool add_macro_calls = false);
  virtual ~ParserVisitor();

  antlrcpp::Any visit(antlr4::tree::ParseTree* tree) override;

  antlrcpp::Any visitStart(
      ::cel_grammar::CelParser::StartContext* ctx) override;
  antlrcpp::Any visitExpr(::cel_grammar::CelParser::ExprContext* ctx) override;
  antlrcpp::Any visitConditionalOr(
      ::cel_grammar::CelParser::ConditionalOrContext* ctx) override;
  antlrcpp::Any visitConditionalAnd(
      ::cel_grammar::CelParser::ConditionalAndContext* ctx) override;
  antlrcpp::Any visitRelation(
      ::cel_grammar::CelParser::RelationContext* ctx) override;
  antlrcpp::Any visitCalc(::cel_grammar::CelParser::CalcContext* ctx) override;
  antlrcpp::Any visitUnary(::cel_grammar::CelParser::UnaryContext* ctx);
  antlrcpp::Any visitLogicalNot(
      ::cel_grammar::CelParser::LogicalNotContext* ctx) override;
  antlrcpp::Any visitNegate(
      ::cel_grammar::CelParser::NegateContext* ctx) override;
  antlrcpp::Any visitSelectOrCall(
      ::cel_grammar::CelParser::SelectOrCallContext* ctx) override;
  antlrcpp::Any visitIndex(
      ::cel_grammar::CelParser::IndexContext* ctx) override;
  antlrcpp::Any visitCreateMessage(
      ::cel_grammar::CelParser::CreateMessageContext* ctx) override;
  antlrcpp::Any visitFieldInitializerList(
      ::cel_grammar::CelParser::FieldInitializerListContext* ctx) override;
  antlrcpp::Any visitIdentOrGlobalCall(
      ::cel_grammar::CelParser::IdentOrGlobalCallContext* ctx) override;
  antlrcpp::Any visitNested(
      ::cel_grammar::CelParser::NestedContext* ctx) override;
  antlrcpp::Any visitCreateList(
      ::cel_grammar::CelParser::CreateListContext* ctx) override;
  std::vector<google::api::expr::v1alpha1::Expr> visitList(
      ::cel_grammar::CelParser::ExprListContext* ctx);
  antlrcpp::Any visitCreateStruct(
      ::cel_grammar::CelParser::CreateStructContext* ctx) override;
  antlrcpp::Any visitConstantLiteral(
      ::cel_grammar::CelParser::ConstantLiteralContext* ctx) override;
  antlrcpp::Any visitPrimaryExpr(
      ::cel_grammar::CelParser::PrimaryExprContext* ctx) override;
  antlrcpp::Any visitMemberExpr(
      ::cel_grammar::CelParser::MemberExprContext* ctx) override;

  antlrcpp::Any visitMapInitializerList(
      ::cel_grammar::CelParser::MapInitializerListContext* ctx) override;
  antlrcpp::Any visitInt(::cel_grammar::CelParser::IntContext* ctx) override;
  antlrcpp::Any visitUint(::cel_grammar::CelParser::UintContext* ctx) override;
  antlrcpp::Any visitDouble(
      ::cel_grammar::CelParser::DoubleContext* ctx) override;
  antlrcpp::Any visitString(
      ::cel_grammar::CelParser::StringContext* ctx) override;
  antlrcpp::Any visitBytes(
      ::cel_grammar::CelParser::BytesContext* ctx) override;
  antlrcpp::Any visitBoolTrue(
      ::cel_grammar::CelParser::BoolTrueContext* ctx) override;
  antlrcpp::Any visitBoolFalse(
      ::cel_grammar::CelParser::BoolFalseContext* ctx) override;
  antlrcpp::Any visitNull(::cel_grammar::CelParser::NullContext* ctx) override;
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

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_VISITOR_H_
