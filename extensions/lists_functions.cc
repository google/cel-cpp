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

#include "extensions/lists_functions.h"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/expr.h"
#include "common/operators.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {
namespace {

// Slow distinct() implementation that uses Equal() to compare values in O(n^2).
absl::Status ListDistinctHeterogeneousImpl(ValueManager& value_manager,
                                           const ListValue& list,
                                           ListValueBuilder& builder,
                                           int64_t start_index = 0,
                                           std::vector<Value> seen = {}) {
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  for (int64_t i = start_index; i < size; ++i) {
    CEL_ASSIGN_OR_RETURN(Value value, list.Get(value_manager, i));
    bool is_distinct = true;
    for (const Value& seen_value : seen) {
      CEL_ASSIGN_OR_RETURN(Value equal, value.Equal(value_manager, seen_value));
      if (equal.IsTrue()) {
        is_distinct = false;
        break;
      }
    }
    if (is_distinct) {
      seen.push_back(value);
      CEL_RETURN_IF_ERROR(builder.Add(value));
    }
  }
  return absl::OkStatus();
}

// Fast distinct() implementation for homogeneous hashable types. Falls back to
// the slow implementation if the list is not actually homogeneous.
template <typename ValueType>
absl::Status ListDistinctHomogeneousHashableImpl(ValueManager& value_manager,
                                                 const ListValue& list,
                                                 ListValueBuilder& builder) {
  absl::flat_hash_set<ValueType> seen;
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  for (int64_t i = 0; i < size; ++i) {
    CEL_ASSIGN_OR_RETURN(Value value, list.Get(value_manager, i));
    if (auto typed_value = value.As<ValueType>(); typed_value.has_value()) {
      if (seen.contains(*typed_value)) {
        continue;
      }
      seen.insert(*typed_value);
      CEL_RETURN_IF_ERROR(builder.Add(value));
    } else {
      // List is not homogeneous, fall back to the slow implementation.
      // Keep the existing list builder, which already constructed the list of
      // all the distinct values (that were homogeneous so far) up to index i.
      // Pass the seen values as a vector to the slow implementation.
      std::vector<Value> seen_values{seen.begin(), seen.end()};
      return ListDistinctHeterogeneousImpl(value_manager, list, builder, i,
                                           std::move(seen_values));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Value> ListDistinct(ValueManager& value_manager,
                                   const ListValue& list) {
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  // If the list is empty or has a single element, we can return it as is.
  if (size < 2) {
    return list;
  }

  // We need a set to keep track of the seen values.
  //
  // By default, for unhashable types, this set is implemented as a vector of
  // all the seen values, which means that we will perform O(n^2) comparisons
  // between the values.
  //
  // For efficiency purposes, if the first element of the list is hashable, we
  // will use a specialized implementation that is faster for homogeneous lists
  // of hashable types.
  // If the list is not homogeneous, we will fall back to the slow
  // implementation.
  //
  // The total runtime cost is O(n) for homogeneous lists of hashable types, and
  // O(n^2) for all other cases.
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  CEL_ASSIGN_OR_RETURN(Value first, list.Get(value_manager, 0));
  switch (first.kind()) {
    case ValueKind::kInt: {
      CEL_RETURN_IF_ERROR(ListDistinctHomogeneousHashableImpl<IntValue>(
          value_manager, list, *builder));
      break;
    }
    case ValueKind::kUint: {
      CEL_RETURN_IF_ERROR(ListDistinctHomogeneousHashableImpl<UintValue>(
          value_manager, list, *builder));
      break;
    }
    case ValueKind::kBool: {
      CEL_RETURN_IF_ERROR(ListDistinctHomogeneousHashableImpl<BoolValue>(
          value_manager, list, *builder));
      break;
    }
    case ValueKind::kString: {
      CEL_RETURN_IF_ERROR(ListDistinctHomogeneousHashableImpl<StringValue>(
          value_manager, list, *builder));
      break;
    }
    default: {
      CEL_RETURN_IF_ERROR(
          ListDistinctHeterogeneousImpl(value_manager, list, *builder));
      break;
    }
  }
  return std::move(*builder).Build();
}

absl::Status ListFlattenImpl(ValueManager& value_manager, const ListValue& list,
                             int64_t remaining_depth,
                             ListValueBuilder& builder) {
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  for (int64_t i = 0; i < size; ++i) {
    CEL_ASSIGN_OR_RETURN(Value value, list.Get(value_manager, i));
    if (absl::optional<ListValue> list_value = value.AsList();
        list_value.has_value() && remaining_depth > 0) {
      CEL_RETURN_IF_ERROR(ListFlattenImpl(value_manager, *list_value,
                                          remaining_depth - 1, builder));
    } else {
      CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Value> ListFlatten(ValueManager& value_manager,
                                  const ListValue& list, int64_t depth = 1) {
  if (depth < 0) {
    return ErrorValue(
        absl::InvalidArgumentError("flatten(): level must be non-negative"));
  }
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  CEL_RETURN_IF_ERROR(ListFlattenImpl(value_manager, list, depth, *builder));
  return std::move(*builder).Build();
}

absl::StatusOr<ListValue> ListRange(ValueManager& value_manager, int64_t end) {
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  builder->Reserve(end);
  for (ssize_t i = 0; i < end; ++i) {
    CEL_RETURN_IF_ERROR(builder->Add(IntValue(i)));
  }
  return std::move(*builder).Build();
}

absl::StatusOr<ListValue> ListReverse(ValueManager& value_manager,
                                      const ListValue& list) {
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  for (ssize_t i = size - 1; i >= 0; --i) {
    CEL_ASSIGN_OR_RETURN(Value value, list.Get(value_manager, i));
    CEL_RETURN_IF_ERROR(builder->Add(value));
  }
  return std::move(*builder).Build();
}

absl::StatusOr<Value> ListSlice(ValueManager& value_manager,
                                const ListValue& list, int64_t start,
                                int64_t end) {
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  if (start < 0 || end < 0) {
    return ErrorValue(absl::InvalidArgumentError(absl::StrFormat(
        "cannot slice(%d, %d), negative indexes not supported", start, end)));
  }
  if (start > end) {
    return cel::ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("cannot slice(%d, %d), start index must be less than "
                        "or equal to end index",
                        start, end)));
  }
  if (size < end) {
    return cel::ErrorValue(absl::InvalidArgumentError(absl::StrFormat(
        "cannot slice(%d, %d), list is length %d", start, end, size)));
  }
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  for (int64_t i = start; i < end; ++i) {
    CEL_ASSIGN_OR_RETURN(Value val, list.Get(value_manager, i));
    CEL_RETURN_IF_ERROR(builder->Add(val));
  }
  return std::move(*builder).Build();
}

template <typename ValueType>
absl::StatusOr<Value> ListSortByAssociatedKeysNative(
    ValueManager& value_manager, const ListValue& list, const ListValue& keys) {
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  // If the list is empty or has a single element, we can return it as is.
  if (size < 2) {
    return list;
  }
  std::vector<ValueType> keys_vec;
  absl::Status status = keys.ForEach(
      value_manager, [&keys_vec](const Value& value) -> absl::StatusOr<bool> {
        if (auto typed_value = value.As<ValueType>(); typed_value.has_value()) {
          keys_vec.push_back(*typed_value);
        } else {
          return absl::InvalidArgumentError(
              "sort(): list elements must have the same type");
        }
        return true;
      });
  if (!status.ok()) {
    return ErrorValue(status);
  }
  ABSL_ASSERT(keys_vec.size() == size);  // Already checked by the caller.
  std::vector<int64_t> sorted_indices(keys_vec.size());
  std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
  std::sort(
      sorted_indices.begin(), sorted_indices.end(),
      [&](int64_t a, int64_t b) -> bool { return keys_vec[a] < keys_vec[b]; });

  // Now sorted_indices contains the indices of the keys in sorted order.
  // We can use it to build the sorted list.
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  for (const auto& index : sorted_indices) {
    CEL_ASSIGN_OR_RETURN(Value value, list.Get(value_manager, index));
    CEL_RETURN_IF_ERROR(builder->Add(value));
  }
  return std::move(*builder).Build();
}

// Internal function used for the implementation of sort() and sortBy().
//
// Sorts a list of arbitrary elements, according to the order produced by
// sorting another list of comparable elements. If the element type of the keys
// is not comparable or the element types are not the same, the function will
// produce an error.
//
//  <list(T)>.@sortByAssociatedKeys(<list(U)>) -> <list(T)>
//  U in {int, uint, double, bool, duration, timestamp, string, bytes}
//
// Example:
//
//  ["foo", "bar", "baz"].@sortByAssociatedKeys([3, 1, 2])
//     -> returns ["bar", "baz", "foo"]
absl::StatusOr<Value> ListSortByAssociatedKeys(ValueManager& value_manager,
                                               const ListValue& list,
                                               const ListValue& keys) {
  CEL_ASSIGN_OR_RETURN(size_t list_size, list.Size());
  CEL_ASSIGN_OR_RETURN(size_t keys_size, keys.Size());
  if (list_size != keys_size) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("@sortByAssociatedKeys() expected a list of the same "
                        "size as the associated keys list, but got %d and %d "
                        "elements respectively.",
                        list_size, keys_size)));
  }
  // Empty lists are already sorted.
  // We don't check for size == 1 because the list could contain a single
  // element of a type that is not supported by this function.
  if (list_size == 0) {
    return list;
  }
  CEL_ASSIGN_OR_RETURN(Value first, keys.Get(value_manager, 0));
  switch (first.kind()) {
    case ValueKind::kInt:
      return ListSortByAssociatedKeysNative<IntValue>(value_manager, list,
                                                      keys);
    case ValueKind::kUint:
      return ListSortByAssociatedKeysNative<UintValue>(value_manager, list,
                                                       keys);
    case ValueKind::kDouble:
      return ListSortByAssociatedKeysNative<DoubleValue>(value_manager, list,
                                                         keys);
    case ValueKind::kBool:
      return ListSortByAssociatedKeysNative<BoolValue>(value_manager, list,
                                                       keys);
    case ValueKind::kString:
      return ListSortByAssociatedKeysNative<StringValue>(value_manager, list,
                                                         keys);
    case ValueKind::kTimestamp:
      return ListSortByAssociatedKeysNative<TimestampValue>(value_manager, list,
                                                            keys);
    case ValueKind::kDuration:
      return ListSortByAssociatedKeysNative<DurationValue>(value_manager, list,
                                                           keys);
    case ValueKind::kBytes:
      return ListSortByAssociatedKeysNative<BytesValue>(value_manager, list,
                                                        keys);
    default:
      return ErrorValue(absl::InvalidArgumentError(
          absl::StrFormat("sort(): unsupported type %s", first.GetTypeName())));
  }
}

// Create an expression equivalent to:
//   target.map(varIdent, mapExpr)
absl::optional<Expr> MakeMapComprehension(MacroExprFactory& factory,
                                          Expr target, Expr var_ident,
                                          Expr map_expr) {
  auto step = factory.NewCall(
      google::api::expr::common::CelOperator::ADD, factory.NewAccuIdent(),
      factory.NewList(factory.NewListElement(std::move(map_expr))));
  auto var_name = var_ident.ident_expr().name();
  return factory.NewComprehension(std::move(var_name), std::move(target),
                                  kAccumulatorVariableName, factory.NewList(),
                                  factory.NewBoolConst(true), std::move(step),
                                  factory.NewAccuIdent());
}

// Create an expression equivalent to:
//   cel.bind(varIdent, varExpr, call_expr)
absl::optional<Expr> MakeBindComprehension(MacroExprFactory& factory,
                                           Expr var_ident, Expr var_expr,
                                           Expr call_expr) {
  auto var_name = var_ident.ident_expr().name();
  return factory.NewComprehension(
      "#unused", factory.NewList(), std::move(var_name), std::move(var_expr),
      factory.NewBoolConst(false), std::move(var_ident), std::move(call_expr));
}

// This macro transforms an expression like:
//
//    mylistExpr.sortBy(e, -math.abs(e))
//
// into something equivalent to:
//
//    cel.bind(
//      @__sortBy_input__,
//      myListExpr,
//      @__sortBy_input__.@sortByAssociatedKeys(
//        @__sortBy_input__.map(e, -math.abs(e)
//      )
//    )
Macro ListSortByMacro() {
  absl::StatusOr<Macro> sortby_macro = Macro::Receiver(
      "sortBy", 2,
      [](MacroExprFactory& factory, Expr& target,
         absl::Span<Expr> args) -> absl::optional<Expr> {
        if (!target.has_ident_expr() && !target.has_select_expr() &&
            !target.has_list_expr() && !target.has_comprehension_expr() &&
            !target.has_call_expr()) {
          return factory.ReportErrorAt(
              target,
              "sortBy can only be applied to a list, identifier, "
              "comprehension, call or select expression");
        }

        auto sortby_input_ident = factory.NewIdent("@__sortBy_input__");
        auto sortby_input_expr = std::move(target);
        auto key_ident = std::move(args[0]);
        auto key_expr = std::move(args[1]);

        // Build the map expression:
        //   map_compr := @__sortBy_input__.map(key_ident, key_expr)
        auto map_compr =
            MakeMapComprehension(factory, factory.Copy(sortby_input_ident),
                                 std::move(key_ident), std::move(key_expr));
        if (!map_compr.has_value()) {
          return absl::nullopt;
        }

        // Build the call expression:
        //   call_expr := @__sortBy_input__.@sortByAssociatedKeys(map_compr)
        std::vector<Expr> call_args;
        call_args.push_back(std::move(*map_compr));
        auto call_expr = factory.NewMemberCall("@sortByAssociatedKeys",
                                               std::move(sortby_input_ident),
                                               absl::MakeSpan(call_args));

        // Build the returned bind expression:
        //   cel.bind(@__sortBy_input__, target, call_expr)
        auto var_ident = factory.NewIdent("@__sortBy_input__");
        auto var_expr = std::move(sortby_input_expr);
        auto bind_compr =
            MakeBindComprehension(factory, std::move(var_ident),
                                  std::move(var_expr), std::move(call_expr));
        return bind_compr;
      });
  return *sortby_macro;
}

absl::StatusOr<Value> ListSort(ValueManager& value_manager,
                               const ListValue& list) {
  return ListSortByAssociatedKeys(value_manager, list, list);
}

absl::Status RegisterListDistinctFunction(FunctionRegistry& registry) {
  return UnaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&>::
      RegisterMemberOverload("distinct", &ListDistinct, registry);
}

absl::Status RegisterListFlattenFunction(FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&,
                             int64_t>::RegisterMemberOverload("flatten",
                                                              &ListFlatten,
                                                              registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&>::
           RegisterMemberOverload(
               "flatten",
               [](ValueManager& value_manager, const ListValue& list) {
                 return ListFlatten(value_manager, list, 1);
               },
               registry)));
  return absl::OkStatus();
}

absl::Status RegisterListRangeFunction(FunctionRegistry& registry) {
  return UnaryFunctionAdapter<absl::StatusOr<Value>,
                              int64_t>::RegisterGlobalOverload("lists.range",
                                                               &ListRange,
                                                               registry);
}

absl::Status RegisterListReverseFunction(FunctionRegistry& registry) {
  return UnaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&>::
      RegisterMemberOverload("reverse", &ListReverse, registry);
}

absl::Status RegisterListSliceFunction(FunctionRegistry& registry) {
  return VariadicFunctionAdapter<absl::StatusOr<Value>, const ListValue&,
                                 int64_t,
                                 int64_t>::RegisterMemberOverload("slice",
                                                                  &ListSlice,
                                                                  registry);
}

absl::Status RegisterListSortFunction(FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&>::
           RegisterMemberOverload("sort", &ListSort, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<
          absl::StatusOr<Value>, const ListValue&,
          const ListValue&>::RegisterMemberOverload("@sortByAssociatedKeys",
                                                    &ListSortByAssociatedKeys,
                                                    registry)));
  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterListsFunctions(FunctionRegistry& registry,
                                    const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterListDistinctFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterListFlattenFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterListRangeFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterListReverseFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterListSliceFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterListSortFunction(registry));
  return absl::OkStatus();
}

std::vector<Macro> lists_macros() { return {ListSortByMacro()}; }

absl::Status RegisterListsMacros(MacroRegistry& registry,
                                 const ParserOptions&) {
  return registry.RegisterMacros(lists_macros());
}

}  // namespace cel::extensions