// Copyright 2024 Google LLC
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
#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_DIRECT_EXPRESSION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_DIRECT_EXPRESSION_STEP_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Represents a directly evaluated CEL expression.
//
// Subexpressions return values on the C++ program stack and call their
// dependencies directly.
//
// This reduces the setup overhead for evaluation and minimizes value churn
// to / from a heap based value stack managed by the CEL runtime, but can't be
// used for arbitrarily nested expressions.
class DirectExpressionStep {
 public:
  virtual ~DirectExpressionStep() = default;

  virtual absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                                AttributeTrail& attribute) = 0;
};

// Wrapper for direct steps to work with the stack machine impl.
class WrappedDirectStep : public ExpressionStep {
 public:
  WrappedDirectStep(std::unique_ptr<DirectExpressionStep> impl, int64_t expr_id)
      : ExpressionStep(expr_id), impl_(std::move(impl)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<DirectExpressionStep>();
  }

 private:
  std::unique_ptr<DirectExpressionStep> impl_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_DIRECT_EXPRESSION_STEP_H_
