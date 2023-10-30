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

#include "extensions/sets_functions.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/function_adapter.h"
#include "base/handle.h"
#include "base/value_factory.h"
#include "base/values/bytes_value.h"
#include "base/values/list_value.h"
#include "base/values/string_value.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

namespace {

absl::StatusOr<Handle<Value>> SetsContains(ValueFactory& value_factory,
                                           const ListValue& list,
                                           const ListValue& sublist) {
  CEL_ASSIGN_OR_RETURN(
      bool any_missing,
      sublist.AnyOf(
          value_factory,
          [&list, &value_factory](
              const Handle<Value>& sublist_element) -> absl::StatusOr<bool> {
            CEL_ASSIGN_OR_RETURN(auto contains,
                                 list.Contains(value_factory, sublist_element));

            // Treat CEL error as missing
            return !contains->Is<BoolValue>() ||
                   !contains->As<BoolValue>().NativeValue();
          }));
  return value_factory.CreateBoolValue(!any_missing);
}

absl::StatusOr<Handle<Value>> SetsIntersects(ValueFactory& value_factory,
                                             const ListValue& list,
                                             const ListValue& sublist) {
  CEL_ASSIGN_OR_RETURN(
      bool exists,
      list.AnyOf(
          value_factory,
          [&value_factory, &sublist](
              const Handle<Value>& list_element) -> absl::StatusOr<bool> {
            CEL_ASSIGN_OR_RETURN(auto contains,
                                 sublist.Contains(value_factory, list_element));
            // Treat contains return CEL error as false for the sake of
            // intersecting.
            return contains->Is<BoolValue>() &&
                   contains->As<BoolValue>().NativeValue();
          }));

  return value_factory.CreateBoolValue(exists);
}

absl::StatusOr<Handle<Value>> SetsEquivalent(ValueFactory& value_factory,
                                             const ListValue& list,
                                             const ListValue& sublist) {
  CEL_ASSIGN_OR_RETURN(auto contains_sublist,
                       SetsContains(value_factory, list, sublist));
  if (contains_sublist->Is<BoolValue>() &&
      !contains_sublist->As<BoolValue>().NativeValue()) {
    return contains_sublist;
  }
  return SetsContains(value_factory, sublist, list);
}

absl::Status RegisterSetsContainsFunction(FunctionRegistry& registry) {
  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Handle<Value>>, const ListValue&,
          const ListValue&>::CreateDescriptor("sets.contains",
                                              /*receiver_style=*/false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, const ListValue&,
                            const ListValue&>::WrapFunction(SetsContains));
}

absl::Status RegisterSetsIntersectsFunction(FunctionRegistry& registry) {
  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Handle<Value>>, const ListValue&,
          const ListValue&>::CreateDescriptor("sets.intersects",
                                              /*receiver_style=*/false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, const ListValue&,
                            const ListValue&>::WrapFunction(SetsIntersects));
}

absl::Status RegisterSetsEquivalentFunction(FunctionRegistry& registry) {
  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Handle<Value>>, const ListValue&,
          const ListValue&>::CreateDescriptor("sets.equivalent",
                                              /*receiver_style=*/false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, const ListValue&,
                            const ListValue&>::WrapFunction(SetsEquivalent));
}

}  // namespace

absl::Status RegisterSetsFunctions(FunctionRegistry& registry,
                                   const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterSetsContainsFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterSetsIntersectsFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterSetsEquivalentFunction(registry));
  return absl::OkStatus();
}

}  // namespace cel::extensions
