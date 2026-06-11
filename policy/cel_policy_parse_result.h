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

#ifndef THIRD_PARTY_CEL_CPP_POLICY_CEL_POLICY_PARSE_RESULT_H_
#define THIRD_PARTY_CEL_CPP_POLICY_CEL_POLICY_PARSE_RESULT_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/source.h"
#include "policy/cel_policy.h"

namespace cel {

class CelPolicyIssue {
 public:
  enum class Severity { kInformation, kDeprecated, kWarning, kError };

  CelPolicyIssue(CelPolicyElementId element_id, absl::string_view message)
      : element_id_(element_id), message_(message) {}
  CelPolicyIssue(CelPolicyElementId element_id, Severity severity,
                 absl::string_view message)
      : element_id_(element_id), severity_(severity), message_(message) {}
  CelPolicyIssue(CelPolicyElementId element_id,
                 SourcePosition relative_position, absl::string_view message)
      : element_id_(element_id),
        relative_position_(relative_position),
        message_(message) {}
  CelPolicyIssue(CelPolicyElementId element_id,
                 SourcePosition relative_position, Severity severity,
                 absl::string_view message)
      : element_id_(element_id),
        relative_position_(relative_position),
        severity_(severity),
        message_(message) {}

  std::string ToDisplayString(
      const CelPolicySource* absl_nullable source) const;
  std::string ToDisplayString(const CelPolicySource& source) const {
    return ToDisplayString(&source);
  }

  Severity severity() const { return severity_; }
  absl::string_view message() const { return message_; }

 private:
  CelPolicyElementId element_id_;
  std::optional<SourcePosition> relative_position_;
  Severity severity_ = Severity::kError;
  std::string message_;
};

class CelPolicyParseResult {
 public:
  explicit CelPolicyParseResult(std::shared_ptr<CelPolicySource> policy_source,
                                std::unique_ptr<CelPolicy> policy,
                                std::vector<CelPolicyIssue> issues)
      : policy_source_(std::move(policy_source)),
        policy_(std::move(policy)),
        issues_(std::move(issues)) {}

  bool IsValid() const { return policy_ != nullptr; }

  const CelPolicy* absl_nullable GetPolicy() const { return policy_.get(); }

  absl::StatusOr<std::unique_ptr<CelPolicy>> ReleasePolicy() {
    if (policy_ == nullptr) {
      return absl::FailedPreconditionError(
          "CelPolicyParseResult is empty. Check for Issues.");
    }
    return std::move(policy_);
  }

  absl::Span<const CelPolicyIssue> GetIssues() const { return issues_; }

  std::string FormattedIssues() const;

 private:
  std::shared_ptr<CelPolicySource> policy_source_;
  absl_nullable std::unique_ptr<CelPolicy> policy_;
  std::vector<CelPolicyIssue> issues_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_POLICY_CEL_POLICY_PARSE_RESULT_H_
