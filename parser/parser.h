#ifndef THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
#define THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "parser/macro.h"
#include "parser/options.h"
#include "parser/source_factory.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

class VerboseParsedExpr {
 public:
  VerboseParsedExpr(google::api::expr::v1alpha1::ParsedExpr parsed_expr,
                    EnrichedSourceInfo enriched_source_info)
      : parsed_expr_(std::move(parsed_expr)),
        enriched_source_info_(std::move(enriched_source_info)) {}

  const google::api::expr::v1alpha1::ParsedExpr& parsed_expr() const {
    return parsed_expr_;
  }
  const EnrichedSourceInfo& enriched_source_info() const {
    return enriched_source_info_;
  }

 private:
  google::api::expr::v1alpha1::ParsedExpr parsed_expr_;
  EnrichedSourceInfo enriched_source_info_;
};

absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description = "<input>",
    const ParserOptions& options = ParserOptions());

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> Parse(
    const std::string& expression, const std::string& description = "<input>",
    const ParserOptions& options = ParserOptions());

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> ParseWithMacros(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description = "<input>",
    const ParserOptions& options = ParserOptions());

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
