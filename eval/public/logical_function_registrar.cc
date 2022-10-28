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

#include "eval/public/logical_function_registrar.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

using ::google::protobuf::Arena;

absl::Status RegisterLogicalFunctions(CelFunctionRegistry* registry,
                                      const InterpreterOptions& options) {
  // logical NOT
  CEL_RETURN_IF_ERROR(
      registry->Register(PortableUnaryFunctionAdapter<bool, bool>::Create(
          builtin::kNot, false,
          [](Arena*, bool value) -> bool { return !value; })));

  // Strictness
  CEL_RETURN_IF_ERROR(
      registry->Register(PortableUnaryFunctionAdapter<bool, bool>::Create(
          builtin::kNotStrictlyFalse, false,
          [](Arena*, bool value) -> bool { return value; })));

  CEL_RETURN_IF_ERROR(registry->Register(
      PortableUnaryFunctionAdapter<bool, const CelError*>::Create(
          builtin::kNotStrictlyFalse, false,
          [](Arena*, const CelError*) -> bool { return true; })));

  CEL_RETURN_IF_ERROR(registry->Register(
      PortableUnaryFunctionAdapter<bool, const UnknownSet*>::Create(
          builtin::kNotStrictlyFalse, false,
          [](Arena*, const UnknownSet*) -> bool { return true; })));

  CEL_RETURN_IF_ERROR(
      registry->Register(PortableUnaryFunctionAdapter<bool, bool>::Create(
          builtin::kNotStrictlyFalseDeprecated, false,
          [](Arena*, bool value) -> bool { return value; })));

  CEL_RETURN_IF_ERROR(registry->Register(
      PortableUnaryFunctionAdapter<bool, const CelError*>::Create(
          builtin::kNotStrictlyFalseDeprecated, false,
          [](Arena*, const CelError*) -> bool { return true; })));

  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
