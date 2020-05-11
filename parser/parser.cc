#include "parser/parser.h"

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

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::ParsedExpr;

cel_base::StatusOr<ParsedExpr> Parse(const std::string& expression,
                                 const std::string& description) {
  return ParseWithMacros(expression, Macro::AllMacros(), description);
}

cel_base::StatusOr<ParsedExpr> ParseWithMacros(const std::string& expression,
                                           const std::vector<Macro>& macros,
                                           const std::string& description) {
  auto result = EnrichedParse(expression, macros, description);
  if (result.ok()) {
    return result->parsed_expr();
  }
  return result.status();
}

cel_base::StatusOr<VerboseParsedExpr> EnrichedParse(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description) {
  ANTLRInputStream input(expression);
  ::cel_grammar::CelLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  ::cel_grammar::CelParser parser(&tokens);

  ParserVisitor visitor(description, expression, macros);

  lexer.removeErrorListeners();
  parser.removeErrorListeners();
  lexer.addErrorListener(&visitor);
  parser.addErrorListener(&visitor);

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
