// Copyright 2023 Google LLC
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
#include "runtime/internal/runtime_impl.h"

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/handle.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "eval/eval/evaluator_core.h"
#include "internal/status_macros.h"
#include "runtime/activation_interface.h"
#include "runtime/runtime.h"

namespace cel::runtime_internal {
namespace {

class ProgramImpl final : public TraceableProgram {
 public:
  using EvaluationListener = TraceableProgram::EvaluationListener;
  ProgramImpl(
      const std::shared_ptr<const RuntimeImpl::Environment>& environment,
      google::api::expr::runtime::FlatExpression impl)
      : environment_(environment), impl_(std::move(impl)) {}

  absl::StatusOr<Handle<Value>> Evaluate(
      const ActivationInterface& activation,
      ValueManager& value_factory) const override {
    return Trace(activation, EvaluationListener(), value_factory);
  }

  absl::StatusOr<Handle<Value>> Trace(
      const ActivationInterface& activation, EvaluationListener callback,
      ValueManager& value_factory) const override {
    auto state = impl_.MakeEvaluatorState(value_factory);
    return impl_.EvaluateWithCallback(activation, std::move(callback), state);
  }

  const TypeProvider& GetTypeProvider() const override {
    return environment_->type_registry.GetComposedTypeProvider();
  }

 private:
  // Keep the Runtime environment alive while programs reference it.
  std::shared_ptr<const RuntimeImpl::Environment> environment_;
  google::api::expr::runtime::FlatExpression impl_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<Program>> RuntimeImpl::CreateProgram(
    std::unique_ptr<Ast> ast,
    const Runtime::CreateProgramOptions& options) const {
  return CreateTraceableProgram(std::move(ast), options);
}

absl::StatusOr<std::unique_ptr<TraceableProgram>>
RuntimeImpl::CreateTraceableProgram(
    std::unique_ptr<Ast> ast,
    const Runtime::CreateProgramOptions& options) const {
  CEL_ASSIGN_OR_RETURN(auto flat_expr, expr_builder_.CreateExpressionImpl(
                                           std::move(ast), options.issues));

  return std::make_unique<ProgramImpl>(environment_, std::move(flat_expr));
}

}  // namespace cel::runtime_internal
