// Copyright 2023 Google LLC
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

#include "eval/eval/cel_expression_flat_impl.h"

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/opaque_value.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/adapter_activation_impl.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::Handle;
using ::cel::Value;
using ::cel::ValueFactory;
using ::cel::extensions::ProtoMemoryManager;

EvaluationListener AdaptListener(const CelEvaluationListener& listener) {
  if (!listener) return nullptr;
  return [&](int64_t expr_id, const Handle<Value>& value,
             ValueFactory& factory) -> absl::Status {
    if (value->Is<cel::OpaqueValue>()) {
      // Opaque types are used to implement some optimized operations.
      // These aren't representable as legacy values and shouldn't be
      // inspectable by clients.
      return absl::OkStatus();
    }
    google::protobuf::Arena* arena =
        ProtoMemoryManager::CastToProtoArena(factory.memory_manager());
    CelValue legacy_value =
        cel::interop_internal::ModernValueToLegacyValueOrDie(arena, value);
    return listener(expr_id, legacy_value, arena);
  };
}
}  // namespace

CelExpressionFlatEvaluationState::CelExpressionFlatEvaluationState(
    google::protobuf::Arena* arena, const FlatExpression& expression)
    : arena_(arena),
      memory_manager_(arena),
      state_(expression.MakeEvaluatorState(memory_manager_)) {}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Trace(
    const BaseActivation& activation, CelEvaluationState* _state,
    CelEvaluationListener callback) const {
  auto state =
      ::cel::internal::down_cast<CelExpressionFlatEvaluationState*>(_state);
  state->state().Reset();
  cel::interop_internal::AdapterActivationImpl modern_activation(activation);

  CEL_ASSIGN_OR_RETURN(
      cel::Handle<cel::Value> value,
      flat_expression_.EvaluateWithCallback(
          modern_activation, AdaptListener(callback), state->state()));

  return cel::interop_internal::ModernValueToLegacyValueOrDie(state->arena(),
                                                              value);
}

std::unique_ptr<CelEvaluationState> CelExpressionFlatImpl::InitializeState(
    google::protobuf::Arena* arena) const {
  return std::make_unique<CelExpressionFlatEvaluationState>(arena,
                                                            flat_expression_);
}

absl::StatusOr<CelValue> CelExpressionFlatImpl::Evaluate(
    const BaseActivation& activation, CelEvaluationState* state) const {
  return Trace(activation, state, CelEvaluationListener());
}

}  // namespace google::api::expr::runtime
