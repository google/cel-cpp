#include "parser/parser.h"

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
#include "parser/cel_grammar.inc/cel_grammar/CelLexer.h"
#include "parser/cel_grammar.inc/cel_grammar/CelParser.h"
#include "parser/source_factory.h"
#include "parser/visitor.h"
#include "antlr4-runtime.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

using antlr4::ANTLRInputStream;
using antlr4::CommonTokenStream;
using antlr4::ParseCancellationException;
using antlr4::ParserRuleContext;

using antlr4::tree::ErrorNode;
using antlr4::tree::TerminalNode;

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::ParsedExpr;

namespace {

// ExprRecursionListener extends the standard ANTLR CelParser to ensure that
// recursive entries into the 'expr' rule are limited to a configurable depth so
// as to prevent stack overflows.
class ExprRecursionListener : public ::antlr4::tree::ParseTreeListener {
 public:
  ExprRecursionListener(
      const int max_recursion_depth = kDefaultMaxRecursionDepth)
      : max_recursion_depth_(max_recursion_depth), recursion_depth_(0) {}

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
  if (ctx->getRuleIndex() == ::cel_grammar::CelParser::RuleExpr) {
    if (recursion_depth_ >= max_recursion_depth_) {
      throw ParseCancellationException(
          absl::StrFormat("Expression recursion limit exceeded. limit: %d",
                          max_recursion_depth_));
    }
    recursion_depth_++;
  }
}

void ExprRecursionListener::exitEveryRule(ParserRuleContext* ctx) {
  if (ctx->getRuleIndex() == ::cel_grammar::CelParser::RuleExpr) {
    recursion_depth_--;
  }
}

}  // namespace

absl::StatusOr<ParsedExpr> Parse(const std::string& expression,
                                 const std::string& description,
                                 const int max_recursion_depth) {
  return ParseWithMacros(expression, Macro::AllMacros(), description,
                         max_recursion_depth);
}

absl::StatusOr<ParsedExpr> ParseWithMacros(const std::string& expression,
                                           const std::vector<Macro>& macros,
                                           const std::string& description,
                                           const int max_recursion_depth) {
  auto result =
      EnrichedParse(expression, macros, description, max_recursion_depth);
  if (result.ok()) {
    return result->parsed_expr();
  }
  return result.status();
}

absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description, const int max_recursion_depth) {
  ANTLRInputStream input(expression);
  ::cel_grammar::CelLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  ::cel_grammar::CelParser parser(&tokens);
  ExprRecursionListener listener(max_recursion_depth);
  ParserVisitor visitor(description, expression, max_recursion_depth, macros);

  lexer.removeErrorListeners();
  parser.removeErrorListeners();
  lexer.addErrorListener(&visitor);
  parser.addErrorListener(&visitor);
  parser.addParseListener(&listener);

  // if we were to ignore errors completely:
  // std::shared_ptr<BailErrorStrategy> error_strategy(new BailErrorStrategy());
  // parser.setErrorHandler(error_strategy);

  ::cel_grammar::CelParser::StartContext* root;
  try {
    root = parser.start();
  } catch (ParseCancellationException& e) {
    return absl::CancelledError(e.what());
  } catch (std::exception& e) {
    return absl::AbortedError(e.what());
  }

  Expr expr = visitor.visit(root).as<Expr>();

  if (visitor.hasErrored()) {
    return absl::InvalidArgumentError(visitor.errorMessage());
  }

  // root is deleted as part of the parser context
  ParsedExpr parsed_expr;
  parsed_expr.mutable_expr()->CopyFrom(expr);
  parsed_expr.mutable_source_info()->CopyFrom(visitor.sourceInfo());
  auto enriched_source_info = visitor.enrichedSourceInfo();
  return VerboseParsedExpr(parsed_expr, enriched_source_info);
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
