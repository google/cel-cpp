// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "policy/cel_policy_parse_result.h"

#include <optional>
#include <string>

#include "absl/base/nullability.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/source.h"
#include "policy/cel_policy.h"

namespace cel {
namespace {

absl::string_view SeverityString(CelPolicyIssue::Severity severity) {
  switch (severity) {
    case CelPolicyIssue::Severity::kInformation:
      return "INFORMATION";
    case CelPolicyIssue::Severity::kWarning:
      return "WARNING";
    case CelPolicyIssue::Severity::kError:
      return "ERROR";
    case CelPolicyIssue::Severity::kDeprecated:
      return "DEPRECATED";
    default:
      return "SEVERITY_UNSPECIFIED";
  }
}

}  // namespace

std::string CelPolicyIssue::ToDisplayString(
    const CelPolicySource* absl_nullable source) const {
  SourceLocation location;
  std::string description;
  std::string snippet;
  if (source != nullptr) {
    if (relative_position_) {
      std::optional<SourceRange> range = source->GetSourceRange(element_id_);
      std::optional<SourcePosition> base =
          source->GetSourcePosition(element_id_);
      if (range.has_value()) {
        base = range->begin;
      }
      if (element_id_ == -1) {
        base.emplace(0);
      }
      if (base) {
        location = source->content()
                       ->GetLocation(*base + *relative_position_)
                       .value_or(SourceLocation{});
      }
    } else {
      location =
          source->GetSourceLocation(element_id_).value_or(SourceLocation{});
    }
    description = std::string(source->content()->description());
    snippet = source->content()->DisplayErrorLocation(location);
  }

  const int display_column = location.column >= 0 ? location.column + 1 : -1;

  return absl::StrFormat("%s: %s:%d:%d: %s%s", SeverityString(severity_),
                         description, location.line, display_column, message_,
                         snippet);
}

std::string CelPolicyParseResult::FormattedIssues() const {
  std::string formatted_issues;
  for (const CelPolicyIssue& issue : issues_) {
    if (!formatted_issues.empty()) {
      absl::StrAppend(&formatted_issues, "\n");
    }
    absl::StrAppend(&formatted_issues, issue.ToDisplayString(*policy_source_));
  }
  return formatted_issues;
}

}  // namespace cel
