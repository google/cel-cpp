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

#include "extensions/comprehensions_v2_functions.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "common/values/map_value_builder.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

namespace {

absl::StatusOr<Value> MapInsert(ValueManager& value_manager,
                                const MapValue& map, const Value& key,
                                const Value& value) {
  if (auto mutable_map_value = common_internal::AsMutableMapValue(map);
      mutable_map_value) {
    // Fast path, runtime has given us a mutable map. We can mutate it directly
    // and return it.
    CEL_RETURN_IF_ERROR(mutable_map_value->Put(key, value))
        .With(ErrorValueReturn());
    return map;
  }
  // Slow path, we have to make a copy.
  auto builder = common_internal::NewMapValueBuilder(
      value_manager.GetMemoryManager().arena());
  if (auto size = map.Size(); size.ok()) {
    builder->Reserve(*size + 1);
  } else {
    size.IgnoreError();
  }
  CEL_RETURN_IF_ERROR(
      map.ForEach(value_manager,
                  [&builder](const Value& key,
                             const Value& value) -> absl::StatusOr<bool> {
                    CEL_RETURN_IF_ERROR(builder->Put(key, value));
                    return true;
                  }))
      .With(ErrorValueReturn());
  CEL_RETURN_IF_ERROR(builder->Put(key, value)).With(ErrorValueReturn());
  return std::move(*builder).Build();
}

}  // namespace

absl::Status RegisterComprehensionsV2Functions(FunctionRegistry& registry,
                                               const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(registry.Register(
      VariadicFunctionAdapter<absl::StatusOr<Value>, MapValue, Value, Value>::
          CreateDescriptor("cel.@mapInsert", /*receiver_style=*/false),
      VariadicFunctionAdapter<absl::StatusOr<Value>, MapValue, Value,
                              Value>::WrapFunction(&MapInsert)));
  return absl::OkStatus();
}

absl::Status RegisterComprehensionsV2Functions(
    google::api::expr::runtime::CelFunctionRegistry* registry,
    const google::api::expr::runtime::InterpreterOptions& options) {
  return RegisterComprehensionsV2Functions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

}  // namespace cel::extensions
