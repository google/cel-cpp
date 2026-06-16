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

#ifndef THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_ISSUE_REPORTER_H_
#define THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_ISSUE_REPORTER_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "common/source.h"
#include "policy/cel_policy.h"
#include "policy/cel_policy_parse_result.h"

namespace cel::policy_internal {

class IssueReporter {
 private:
  using Severity = CelPolicyIssue::Severity;

 public:
  void ReportIssue(CelPolicyElementId element, Severity severity,
                   absl::string_view message);

  void ReportOffsetIssue(CelPolicyElementId element,
                         cel::SourcePosition relative_position,
                         Severity severity, absl::string_view message);

  void ReportError(CelPolicyElementId element, absl::string_view message);
  void ReportError(CelPolicyElementId element, SourcePosition relative_pos,
                   absl::string_view message);

  std::vector<CelPolicyIssue> ReleaseIssues() {
    using std::swap;
    std::vector<CelPolicyIssue> out;
    swap(out, issues_);
    return out;
  }
  const std::vector<CelPolicyIssue>& issues() const { return issues_; }

 private:
  std::vector<CelPolicyIssue> issues_;
};

}  // namespace cel::policy_internal

#endif  // THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_ISSUE_REPORTER_H_
