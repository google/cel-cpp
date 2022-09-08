// Copyright 2022 Google LLC
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

#include "eval/eval/regex_match_step.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "eval/eval/expression_step_base.h"
#include "re2/re2.h"

namespace google::api::expr::runtime {

namespace {

inline constexpr int kNumRegexMatchArguments = 2;

class RegexMatchStep final : public ExpressionStepBase {
 public:
  RegexMatchStep(int64_t expr_id, std::shared_ptr<const RE2> re2)
      : ExpressionStepBase(expr_id, /*comes_from_ast=*/true),
        re2_(std::move(re2)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(kNumRegexMatchArguments)) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Insufficient arguments supplied for regular "
                          "expression match");
    }
    auto input_args = frame->value_stack().GetSpan(kNumRegexMatchArguments);
    const auto& subject = input_args[0];
    const auto& pattern = input_args[1];
    if (!subject.IsString()) {
      return absl::Status(absl::StatusCode::kInternal,
                          "First argument for regular "
                          "expression match must be a string");
    }
    if (!pattern.IsString()) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Second argument for regular "
                          "expression match must be a string");
    }
    if (re2_->pattern() != pattern.StringOrDie().value()) {
      return absl::Status(
          absl::StatusCode::kInternal,
          "Original pattern and supplied pattern are not the same");
    }
    bool match = RE2::PartialMatch(re2::StringPiece(subject.StringOrDie().value().data(), subject.StringOrDie().value().size()), *re2_);
    frame->value_stack().Pop(kNumRegexMatchArguments);
    frame->value_stack().Push(CelValue::CreateBool(match));
    return absl::OkStatus();
  }

 private:
  const std::shared_ptr<const RE2> re2_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateRegexMatchStep(
    std::shared_ptr<const RE2> re2, int64_t expr_id) {
  return std::make_unique<RegexMatchStep>(expr_id, std::move(re2));
}

}  // namespace google::api::expr::runtime
