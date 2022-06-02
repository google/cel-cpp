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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_SOURCE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_PARSER_SOURCE_FACTORY_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "antlr4-runtime.h"
#include "parser/internal/CelParser.h"

namespace google::api::expr::parser {

using google::api::expr::v1alpha1::Expr;

class EnrichedSourceInfo {
 public:
  explicit EnrichedSourceInfo(
      std::map<int64_t, std::pair<int32_t, int32_t>> offsets)
      : offsets_(std::move(offsets)) {}

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
        : message(std::move(message)), location(location) {}
    std::string message;
    SourceLocation location;
  };

  enum QuantifierKind {
    QUANTIFIER_ALL,
    QUANTIFIER_EXISTS,
    QUANTIFIER_EXISTS_ONE
  };

  explicit SourceFactory(absl::string_view expression);

  int64_t Id(const antlr4::Token* token);
  int64_t Id(antlr4::ParserRuleContext* ctx);
  int64_t Id(const SourceLocation& location);

  int64_t NextMacroId(int64_t macro_id);

  const SourceLocation& GetSourceLocation(int64_t id) const;

  static const SourceLocation NoLocation();

  Expr NewExpr(int64_t id);
  Expr NewExpr(antlr4::ParserRuleContext* ctx);
  Expr NewExpr(const antlr4::Token* token);
  Expr NewGlobalCall(int64_t id, const std::string& function,
                     const std::vector<Expr>& args);
  Expr NewGlobalCallForMacro(int64_t macro_id, const std::string& function,
                             const std::vector<Expr>& args);
  Expr NewReceiverCall(int64_t id, const std::string& function,
                       const Expr& target, const std::vector<Expr>& args);
  Expr NewIdent(const antlr4::Token* token, const std::string& ident_name);
  Expr NewIdentForMacro(int64_t macro_id, const std::string& ident_name);
  Expr NewSelect(::cel_parser_internal::CelParser::SelectOrCallContext* ctx,
                 Expr& operand, const std::string& field);
  Expr NewPresenceTestForMacro(int64_t macro_id, const Expr& operand,
                               const std::string& field);
  Expr NewObject(int64_t obj_id, const std::string& type_name,
                 const std::vector<Expr::CreateStruct::Entry>& entries);
  Expr::CreateStruct::Entry NewObjectField(int64_t field_id,
                                           const std::string& field,
                                           const Expr& value);
  Expr NewComprehension(int64_t id, const std::string& iter_var,
                        const Expr& iter_range, const std::string& accu_var,
                        const Expr& accu_init, const Expr& condition,
                        const Expr& step, const Expr& result);

  Expr FoldForMacro(int64_t macro_id, const std::string& iter_var,
                    const Expr& iter_range, const std::string& accu_var,
                    const Expr& accu_init, const Expr& condition,
                    const Expr& step, const Expr& result);
  Expr NewQuantifierExprForMacro(QuantifierKind kind, int64_t macro_id,
                                 const Expr& target,
                                 const std::vector<Expr>& args);
  Expr NewFilterExprForMacro(int64_t macro_id, const Expr& target,
                             const std::vector<Expr>& args);

  Expr NewList(int64_t list_id, const std::vector<Expr>& elems);
  Expr NewListForMacro(int64_t macro_id, const std::vector<Expr>& elems);
  Expr NewMap(int64_t map_id,
              const std::vector<Expr::CreateStruct::Entry>& entries);
  Expr NewMapForMacro(int64_t macro_id, const Expr& target,
                      const std::vector<Expr>& args);
  Expr::CreateStruct::Entry NewMapEntry(int64_t entry_id, const Expr& key,
                                        const Expr& value);
  Expr NewLiteralInt(antlr4::ParserRuleContext* ctx, int64_t value);
  Expr NewLiteralIntForMacro(int64_t macro_id, int64_t value);
  Expr NewLiteralUint(antlr4::ParserRuleContext* ctx, uint64_t value);
  Expr NewLiteralDouble(antlr4::ParserRuleContext* ctx, double value);
  Expr NewLiteralString(antlr4::ParserRuleContext* ctx, const std::string& s);
  Expr NewLiteralBytes(antlr4::ParserRuleContext* ctx, const std::string& b);
  Expr NewLiteralBool(antlr4::ParserRuleContext* ctx, bool b);
  Expr NewLiteralBoolForMacro(int64_t macro_id, bool b);
  Expr NewLiteralNull(antlr4::ParserRuleContext* ctx);

  Expr ReportError(antlr4::ParserRuleContext* ctx, absl::string_view msg);
  Expr ReportError(int32_t line, int32_t col, absl::string_view msg);
  Expr ReportError(const SourceLocation& loc, absl::string_view msg);

  bool IsReserved(absl::string_view ident_name);
  google::api::expr::v1alpha1::SourceInfo source_info() const;
  EnrichedSourceInfo enriched_source_info() const;
  const std::vector<Error>& errors() const { return errors_truncated_; }
  std::string ErrorMessage(absl::string_view description,
                           absl::string_view expression) const;

  Expr BuildArgForMacroCall(const Expr& expr);
  void AddMacroCall(int64_t macro_id, const Expr& target,
                    const std::vector<Expr>& args, std::string function);

 private:
  void CalcLineOffsets(absl::string_view expression);
  absl::optional<int32_t> FindLineOffset(int32_t line) const;
  std::string GetSourceLine(int32_t line, absl::string_view expression) const;

 private:
  int64_t next_id_;
  std::map<int64_t, SourceLocation> positions_;
  // Truncated at kMaxErrorsToReport.
  std::vector<Error> errors_truncated_;
  int64_t num_errors_;
  std::vector<int32_t> line_offsets_;
  std::map<int64_t, Expr> macro_calls_;
};

}  // namespace google::api::expr::parser

#endif  // THIRD_PARTY_CEL_CPP_PARSER_SOURCE_FACTORY_H_
