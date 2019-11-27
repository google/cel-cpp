#include "parser/parser.h"

#include "absl/types/optional.h"
#include "parser/cel_grammar.inc/cel_grammar/CelLexer.h"
#include "parser/cel_grammar.inc/cel_grammar/CelParser.h"
#include "parser/visitor.h"
#include "antlr4-runtime.h"
#include "base/canonical_errors.h"

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
    return cel_base::CancelledError(e.what());
  } catch (std::exception& e) {
    return cel_base::AbortedError(e.what());
  }

  Expr expr = visitor.visit(root).as<Expr>();

  if (visitor.hasErrored()) {
    return cel_base::InvalidArgumentError(visitor.errorMessage());
  }

  // root is deleted as part of the parser context
  ParsedExpr parsed_expr;
  parsed_expr.mutable_expr()->CopyFrom(expr);
  parsed_expr.mutable_source_info()->CopyFrom(visitor.sourceInfo());
  return parsed_expr;
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
