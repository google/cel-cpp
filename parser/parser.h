#ifndef THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
#define THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/types/optional.h"
#include "parser/macro.h"
#include "base/statusor.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

cel_base::StatusOr<google::api::expr::v1alpha1::ParsedExpr> Parse(
    const std::string& expression, const std::string& description = "<input>");

cel_base::StatusOr<google::api::expr::v1alpha1::ParsedExpr> ParseWithMacros(
    const std::string& expression, const std::vector<Macro>& macros,
    const std::string& description = "<input>");

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
