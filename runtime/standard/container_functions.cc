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

#include "runtime/standard/container_functions.h"

#include <utility>

#include "absl/status/status.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/internal/mutable_list_impl.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

using cel::runtime_internal::MutableListValue;

int64_t MapSizeImpl(ValueFactory&, const MapValue& value) {
  return value.size();
}

int64_t ListSizeImpl(ValueFactory&, const ListValue& value) {
  return value.size();
}

// Concatenation for CelList type.
absl::StatusOr<Handle<ListValue>> ConcatList(ValueFactory& factory,
                                             const Handle<ListValue>& value1,
                                             const Handle<ListValue>& value2) {
  int size1 = value1->size();
  if (size1 == 0) {
    return value2;
  }
  int size2 = value2->size();
  if (size2 == 0) {
    return value1;
  }

  // TODO(uncreated-issue/50): add option for checking lists have homogenous element
  // types and use a more specialized list type when possible.
  CEL_ASSIGN_OR_RETURN(Handle<ListType> list_type,
                       factory.type_factory().CreateListType(
                           factory.type_factory().GetDynType()));
  CEL_ASSIGN_OR_RETURN(auto list_builder, list_type->NewValueBuilder(factory));

  list_builder->reserve(size1 + size2);

  ListValue::GetContext context(factory);
  for (int i = 0; i < size1; i++) {
    CEL_ASSIGN_OR_RETURN(Handle<Value> elem, value1->Get(context, i));
    CEL_RETURN_IF_ERROR(list_builder->Add(std::move(elem)));
  }
  for (int i = 0; i < size2; i++) {
    CEL_ASSIGN_OR_RETURN(Handle<Value> elem, value2->Get(context, i));
    CEL_RETURN_IF_ERROR(list_builder->Add(std::move(elem)));
  }

  return std::move(*list_builder).Build();
}

// AppendList will append the elements in value2 to value1.
//
// This call will only be invoked within comprehensions where `value1` is an
// intermediate result which cannot be directly assigned or co-mingled with a
// user-provided list.
absl::StatusOr<Handle<OpaqueValue>> AppendList(ValueFactory& factory,
                                               Handle<OpaqueValue> value1,
                                               const ListValue& value2) {
  // The `value1` object cannot be directly addressed and is an intermediate
  // variable. Once the comprehension completes this value will in effect be
  // treated as immutable.
  if (!value1->Is<MutableListValue>()) {
    return absl::InvalidArgumentError(
        "Unexpected call to runtime list append.");
  }
  MutableListValue& mutable_list =
      const_cast<MutableListValue&>(value1->As<MutableListValue>());
  ListValue::GetContext context(factory);
  for (int i = 0; i < value2.size(); i++) {
    CEL_ASSIGN_OR_RETURN(Handle<Value> elem, value2.Get(context, i));
    CEL_RETURN_IF_ERROR(mutable_list.Append(std::move(elem)));
  }
  return value1;
}
}  // namespace

absl::Status RegisterContainerFunctions(FunctionRegistry& registry,
                                        const RuntimeOptions& options) {
  // receiver style = true/false
  // Support both the global and receiver style size() for lists and maps.
  for (bool receiver_style : {true, false}) {
    CEL_RETURN_IF_ERROR(registry.Register(
        cel::UnaryFunctionAdapter<int64_t, const ListValue&>::CreateDescriptor(
            cel::builtin::kSize, receiver_style),
        UnaryFunctionAdapter<int64_t, const ListValue&>::WrapFunction(
            ListSizeImpl)));

    CEL_RETURN_IF_ERROR(registry.Register(
        UnaryFunctionAdapter<int64_t, const MapValue&>::CreateDescriptor(
            cel::builtin::kSize, receiver_style),
        UnaryFunctionAdapter<int64_t, const MapValue&>::WrapFunction(
            MapSizeImpl)));
  }

  if (options.enable_list_concat) {
    CEL_RETURN_IF_ERROR(registry.Register(
        BinaryFunctionAdapter<
            absl::StatusOr<Handle<Value>>, const ListValue&,
            const ListValue&>::CreateDescriptor(cel::builtin::kAdd, false),
        BinaryFunctionAdapter<
            absl::StatusOr<Handle<Value>>, const Handle<ListValue>&,
            const Handle<ListValue>&>::WrapFunction(ConcatList)));
  }

  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Handle<OpaqueValue>>, Handle<OpaqueValue>,
          const ListValue&>::CreateDescriptor(cel::builtin::kRuntimeListAppend,
                                              false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<OpaqueValue>>,
                            Handle<OpaqueValue>,
                            const ListValue&>::WrapFunction(AppendList));
}

}  // namespace cel
