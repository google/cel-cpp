#ifndef THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
#define THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "parser/macro.h"
#include "parser/source_factory.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

constexpr int kDefaultMaxRecursionDepth = 250;

class VerboseParsedExpr {
 public:
  VerboseParsedExpr(const google::api::expr::v1alpha1::ParsedExpr& parsed_expr,
                    const EnrichedSourceInfo& enriched_source_info)
      : parsed_expr_(parsed_expr),
        enriched_source_info_(enriched_source_info) {}

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
    int max_recursion_depth = kDefaultMaxRecursionDepth);

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> Parse(
    const std::string& expression, const std::string& description = "<input>",
    int max_recursion_depth = kDefaultMaxRecursionDepth);

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> ParseWithMacros(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description = "<input>",
    int max_recursion_depth = kDefaultMaxRecursionDepth);

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
