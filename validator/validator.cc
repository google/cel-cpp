// Copyright 2026 Google LLC
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

#include "validator/validator.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "checker/type_check_issue.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/source.h"

namespace cel {

void Validator::AddValidation(Validation validation) {
  ABSL_DCHECK(validation);
  if (!validation) return;
  validations_.push_back(std::move(validation));
}

Validator::ValidationOutput Validator::Validate(const Ast& ast) const {
  ValidationOutput result;
  ValidationContext context(ast);
  for (const auto& validation : validations_) {
    if (!validation(context)) {
      result.valid = false;
    }
  }
  result.issues = context.ReleaseIssues();
  return result;
}

void Validator::UpdateValidationResult(ValidationResult& in) const {
  if (!in.IsValid() || in.GetAst() == nullptr) {
    // If the result is already decided invalid, just return it.
    return;
  }

  auto result = Validate(*in.GetAst());
  if (!result.valid) {
    in.ReleaseAst().IgnoreError();
  }
  for (auto& issue : result.issues) {
    in.AddIssue(std::move(issue));
  }
}

void ValidationContext::ReportWarningAt(int64_t id, absl::string_view message) {
  issues_.push_back(TypeCheckIssue(TypeCheckIssue::Severity::kWarning,
                                   ast_.ComputeSourceLocation(id),
                                   std::string(message)));
}

void ValidationContext::ReportErrorAt(int64_t id, absl::string_view message) {
  issues_.push_back(TypeCheckIssue(TypeCheckIssue::Severity::kError,
                                   ast_.ComputeSourceLocation(id),
                                   std::string(message)));
}

void ValidationContext::ReportWarning(absl::string_view message) {
  issues_.push_back(TypeCheckIssue(TypeCheckIssue::Severity::kWarning,
                                   SourceLocation{}, std::string(message)));
}

void ValidationContext::ReportError(absl::string_view message) {
  issues_.push_back(TypeCheckIssue(TypeCheckIssue::Severity::kError,
                                   SourceLocation{}, std::string(message)));
}

}  // namespace cel
