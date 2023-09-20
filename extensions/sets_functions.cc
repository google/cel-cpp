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
  CEL_ASSIGN_OR_RETURN(auto sublist_iterator,
                       sublist.NewIterator(value_factory));
  while (sublist_iterator->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto sublist_element, sublist_iterator->NextValue());
    CEL_ASSIGN_OR_RETURN(auto contains,
                         list.Contains(value_factory, sublist_element));
    if (contains->Is<BoolValue>() && !contains->As<BoolValue>().value()) {
      return contains;
    }
  }
  return value_factory.CreateBoolValue(true);
}

absl::StatusOr<Handle<Value>> SetsIntersects(ValueFactory& value_factory,
                                             const ListValue& list,
                                             const ListValue& sublist) {
  CEL_ASSIGN_OR_RETURN(auto list_iterator, list.NewIterator(value_factory));
  while (list_iterator->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto list_element, list_iterator->NextValue());
    CEL_ASSIGN_OR_RETURN(auto contains,
                         sublist.Contains(value_factory, list_element));
    if (contains->Is<BoolValue>() && contains->As<BoolValue>().value()) {
      return contains;
    }
  }
  return value_factory.CreateBoolValue(false);
}

absl::StatusOr<Handle<Value>> SetsEquivalent(ValueFactory& value_factory,
                                             const ListValue& list,
                                             const ListValue& sublist) {
  CEL_ASSIGN_OR_RETURN(auto contains_sublist,
                       SetsContains(value_factory, list, sublist));
  if (contains_sublist->Is<BoolValue>() &&
      !contains_sublist->As<BoolValue>().value()) {
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
