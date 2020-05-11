#ifndef THIRD_PARTY_CEL_CPP_PARSER_SOURCE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_PARSER_SOURCE_FACTORY_H_

#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "parser/cel_grammar.inc/cel_grammar/CelParser.h"
#include "antlr4-runtime.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

using google::api::expr::v1alpha1::Expr;

class EnrichedSourceInfo {
 public:
  EnrichedSourceInfo(const std::map<int64_t, std::pair<int32_t, int32_t>>& offsets)
      : offsets_(offsets) {}

  const std::map<int64_t, std::pair<int32_t, int32_t>>& offsets() const {
    return offsets_;
  }

 private:
  // A map between node_id and pair of start position and end position
  std::map<int64_t, std::pair<int32_t, int32_t>> offsets_;
};

// Provide tools to generate expressions during parsing.
// Keeps track of ID and source location information.
// Shares functionality with //third_party/cel/go/parser/helper.go
class SourceFactory {
 public:
  struct SourceLocation {
    SourceLocation(int32_t line, int32_t col, int32_t offset_end,
                   const std::vector<int32_t>& line_offsets)
        : line(line), col(col), offset_end(offset_end) {
      if (line == 1) {
        offset = col;
      } else if (line > 1) {
        offset = line_offsets[line - 2] + col;
      } else {
        offset = -1;
      }
    }
    int32_t line;
    int32_t col;
    int32_t offset_end;
    int32_t offset;
  };

  struct Error {
    Error(std::string message, SourceLocation location)
        : message(message), location(location) {}
    std::string message;
    SourceLocation location;
  };

  enum QuantifierKind {
    QUANTIFIER_ALL,
    QUANTIFIER_EXISTS,
    QUANTIFIER_EXISTS_ONE
  };

  SourceFactory(const std::string& expression);

  int64_t id(const antlr4::Token* token);
  int64_t id(antlr4::ParserRuleContext* ctx);
  int64_t id(const SourceLocation& location);

  int64_t nextMacroId(int64_t macro_id);

  const SourceLocation& getSourceLocation(int64_t id) const;

  Expr newExpr(int64_t id);
  Expr newExpr(antlr4::ParserRuleContext* ctx);
  Expr newExpr(const antlr4::Token* token);
  Expr newGlobalCall(int64_t id, const std::string& function,
                     const std::vector<Expr>& args);
  Expr newGlobalCallForMacro(int64_t macro_id, const std::string& function,
                             const std::vector<Expr>& args);
  Expr newReceiverCall(int64_t id, const std::string& function, Expr& target,
                       const std::vector<Expr>& args);
  Expr newIdent(const antlr4::Token* token, const std::string& ident_name);
  Expr newIdentForMacro(int64_t macro_id, const std::string& ident_name);
  Expr newSelect(::cel_grammar::CelParser::SelectOrCallContext* ctx,
                 Expr& operand, const std::string& field);
  Expr newPresenceTestForMacro(int64_t macro_id, const Expr& operand,
                               const std::string& field);
  Expr newObject(int64_t obj_id, std::string type_name,
                 const std::vector<Expr::CreateStruct::Entry>& entries);
  Expr::CreateStruct::Entry newObjectField(int64_t field_id,
                                           const std::string& field,
                                           const Expr& value);
  Expr newComprehension(int64_t id, const std::string& iter_var,
                        const Expr& iter_range, const std::string& accu_var,
                        const Expr& accu_init, const Expr& condition,
                        const Expr& step, const Expr& result);

  Expr foldForMacro(int64_t macro_id, const std::string& iter_var,
                    const Expr& iter_range, const std::string& accu_var,
                    const Expr& accu_init, const Expr& condition,
                    const Expr& step, const Expr& result);
  Expr newQuantifierExprForMacro(QuantifierKind kind, int64_t macro_id,
                                 Expr* target, const std::vector<Expr>& args);
  Expr newFilterExprForMacro(int64_t macro_id, Expr* target,
                             const std::vector<Expr>& args);

  Expr newList(int64_t list_id, const std::vector<Expr>& elems);
  Expr newListForMacro(int64_t macro_id, const std::vector<Expr>& elems);
  Expr newMap(int64_t map_id,
              const std::vector<Expr::CreateStruct::Entry>& entries);
  Expr newMapForMacro(int64_t macro_id, Expr* target,
                      const std::vector<Expr>& args);
  Expr::CreateStruct::Entry newMapEntry(int64_t entry_id, const Expr& key,
                                        const Expr& value);
  Expr newLiteralInt(antlr4::ParserRuleContext* ctx, int64_t value);
  Expr newLiteralIntForMacro(int64_t macro_id, int64_t value);
  Expr newLiteralUint(antlr4::ParserRuleContext* ctx, uint64_t value);
  Expr newLiteralDouble(antlr4::ParserRuleContext* ctx, double value);
  Expr newLiteralString(antlr4::ParserRuleContext* ctx, const std::string& s);
  Expr newLiteralBytes(antlr4::ParserRuleContext* ctx, const std::string& b);
  Expr newLiteralBool(antlr4::ParserRuleContext* ctx, bool b);
  Expr newLiteralBoolForMacro(int64_t macro_id, bool b);
  Expr newLiteralNull(antlr4::ParserRuleContext* ctx);

  Expr reportError(antlr4::ParserRuleContext* ctx, const std::string& msg);
  Expr reportError(int32_t line, int32_t col, const std::string& msg);
  Expr reportError(const SourceLocation& loc, const std::string& msg);

  bool isReserved(const std::string& ident_name);
  google::api::expr::v1alpha1::SourceInfo sourceInfo() const;
  EnrichedSourceInfo enrichedSourceInfo() const;
  const std::vector<int32_t>& line_offsets() const;
  const std::vector<Error>& errors() const { return errors_; }

 private:
  void calcLineOffsets(const std::string& expression);

 private:
  int64_t next_id_;
  std::map<int64_t, SourceLocation> positions_;
  std::vector<Error> errors_;
  std::vector<int32_t> line_offsets_;
};

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_SOURCE_FACTORY_H_
