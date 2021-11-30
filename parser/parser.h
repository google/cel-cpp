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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
#define THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "parser/macro.h"
#include "parser/options.h"
#include "parser/source_factory.h"

namespace google::api::expr::parser {

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
    absl::string_view expression, const std::vector<Macro>& macros,
    absl::string_view description = "<input>",
    const ParserOptions& options = ParserOptions());

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> Parse(
    absl::string_view expression, absl::string_view description = "<input>",
    const ParserOptions& options = ParserOptions());

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> ParseWithMacros(
    absl::string_view expression, const std::vector<Macro>& macros,
    absl::string_view description = "<input>",
    const ParserOptions& options = ParserOptions());

}  // namespace google::api::expr::parser

#endif  // THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
