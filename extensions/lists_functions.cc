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
#include <variant>
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

absl::StatusOr<Value> ListDistinct(ValueManager& value_manager,
                                   const ListValue& list) {
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  // If the list is empty or has a single element, we can return it as is.
  if (size < 2) {
    return list;
  }

  // We use a set to keep track of the seen values.
  //
  // By default, for unhashable types, this set is implemented as a vector of
  // all the seen values, which means that we will perform O(n^2) comparisons
  // between the values.
  //
  // For efficiency purposes, we also keep track of the seen values whose type
  // is hashable in a flat_hash_set. This means that if we run distinct() only
  // on hashable values, the runtime will be O(n) instead of O(n^2).
  std::vector<Value> seen_unhashable_values;
  absl::flat_hash_set<std::variant<IntValue, UintValue, BoolValue, StringValue>>
      seen_hashable_values;

  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  absl::Status status = list.ForEach(
      value_manager, [&](const Value& value) -> absl::StatusOr<bool> {
        // Fast path for hashable types.
        switch (value.kind()) {
          case ValueKind::kInt: {
            IntValue int_value = value.GetInt();
            if (seen_hashable_values.contains(int_value)) {
              return true;
            }
            seen_hashable_values.insert(int_value);
            break;
          }
          case ValueKind::kUint: {
            UintValue uint_value = value.GetUint();
            if (seen_hashable_values.contains(uint_value)) {
              return true;
            }
            seen_hashable_values.insert(uint_value);
            break;
          }
          case ValueKind::kBool: {
            BoolValue bool_value = value.GetBool();
            if (seen_hashable_values.contains(bool_value)) {
              return true;
            }
            seen_hashable_values.insert(bool_value);
            break;
          }
          case ValueKind::kString: {
            StringValue string_value = value.GetString();
            if (seen_hashable_values.contains(string_value)) {
              return true;
            }
            seen_hashable_values.insert(string_value);
            break;
          }
          default:
            break;
        }
        // Otherwise, iterate over the seen unhashable values and compare.
        for (const Value& seen_value : seen_unhashable_values) {
          CEL_ASSIGN_OR_RETURN(Value equal,
                               value.Equal(value_manager, seen_value));
          if (equal.IsTrue()) {
            return true;
          }
        }
        seen_unhashable_values.push_back(value);
        CEL_RETURN_IF_ERROR(builder->Add(value));
        return true;
      });
  if (!status.ok()) {
    return ErrorValue(status);
  }
  return std::move(*builder).Build();
}

absl::StatusOr<Value> ListFlatten(ValueManager& value_manager,
                                  const ListValue& list, int64_t depth = 1) {
  if (depth < 0) {
    return ErrorValue(
        absl::InvalidArgumentError("flatten(): level must be non-negative"));
  }
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  CEL_ASSIGN_OR_RETURN(size_t size, list.Size());
  for (int64_t i = 0; i < size; ++i) {
    CEL_ASSIGN_OR_RETURN(Value value, list.Get(value_manager, i));
    if (absl::optional<ListValue> list_value = value.AsList();
        list_value.has_value() && depth > 0) {
      CEL_ASSIGN_OR_RETURN(Value flattened_value,
                           ListFlatten(value_manager, *list_value, depth - 1));
      if (auto flattened_error_value = flattened_value.AsError();
          flattened_error_value.has_value()) {
        return *flattened_error_value;
      } else if (auto flattened_list_value = flattened_value.AsList();
                 flattened_list_value.has_value()) {
        CEL_RETURN_IF_ERROR(flattened_list_value->ForEach(
            value_manager, [&](const Value& value) -> absl::StatusOr<bool> {
              CEL_RETURN_IF_ERROR(builder->Add(value));
              return true;
            }));
      } else {
        return absl::InternalError(
            "flatten(): unexpected value type returned by recursive call");
      }
    } else {
      CEL_RETURN_IF_ERROR(builder->Add(std::move(value)));
    }
  }
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
        absl::StrFormat("sortedByAssociatedKeys() expected a list of the same "
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
    // TODO: add support for duration, timestamp and bytes, which
    // are not yet comparable in C++.
    default:
      return ErrorValue(absl::InvalidArgumentError(absl::StrFormat(
          "sorted() does not support type %s.", first.GetTypeName())));
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
//  )
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
