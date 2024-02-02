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

#include "runtime/standard/logical_functions.h"

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/value_manager.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"
#include "runtime/register_function_helper.h"

namespace cel {
namespace {

using ::cel::runtime_internal::CreateNoMatchingOverloadError;

Value NotStrictlyFalseImpl(ValueManager& value_factory, const Value& value) {
  if (value->Is<BoolValue>()) {
    return value;
  }

  if (value->Is<ErrorValue>() || value->Is<UnknownValue>()) {
    return value_factory.CreateBoolValue(true);
  }

  // Should only accept bool unknown or error.
  return value_factory.CreateErrorValue(
      CreateNoMatchingOverloadError(builtin::kNotStrictlyFalse));
}

}  // namespace

absl::Status RegisterLogicalFunctions(FunctionRegistry& registry,
                                      const RuntimeOptions& options) {
  // logical NOT
  CEL_RETURN_IF_ERROR(
      (RegisterHelper<UnaryFunctionAdapter<bool, bool>>::RegisterGlobalOverload(
          builtin::kNot,
          [](ValueManager&, bool value) -> bool { return !value; }, registry)));

  // Strictness
  using StrictnessHelper = RegisterHelper<UnaryFunctionAdapter<Value, Value>>;
  CEL_RETURN_IF_ERROR(StrictnessHelper::RegisterNonStrictOverload(
      builtin::kNotStrictlyFalse, &NotStrictlyFalseImpl, registry));

  CEL_RETURN_IF_ERROR(StrictnessHelper::RegisterNonStrictOverload(
      builtin::kNotStrictlyFalseDeprecated, &NotStrictlyFalseImpl, registry));

  return absl::OkStatus();
}

}  // namespace cel
