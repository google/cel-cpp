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
//
// Definitions for template helpers to wrap C++ functions as CEL extension
// function implementations.
// TODO(uncreated-issue/20): Add generalized version in addition to the common cases
// of unary/binary functions.

#ifndef THIRD_PARTY_CEL_CPP_BASE_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_BASE_FUNCTION_ADAPTER_H_

#include <functional>
#include <memory>

#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "base/handle.h"
#include "base/internal/function_adapter.h"
#include "base/value.h"
#include "internal/status_macros.h"

namespace cel {
namespace internal {

template <typename T>
struct AdaptedTypeTraits {
  using AssignableType = T;

  static T ToArg(AssignableType v) { return v; }
};

// Specialization for cref parameters without forcing a temporary copy of the
// underlying handle argument.
template <typename T>
struct AdaptedTypeTraits<const T&> {
  using AssignableType = const T*;

  static const T& ToArg(AssignableType v) { return *ABSL_DIE_IF_NULL(v); }
};

}  // namespace internal

// Adapter class for generating CEL extension functions from a two argument
// function. Generates an implementation of the cel::Function interface that
// calls the function to wrap.
//
// Extension functions must distinguish between recoverable errors (error that
// should participate in CEL's error pruning) and unrecoverable errors (a non-ok
// absl::Status that stops evaluation). The function to wrap may return
// StatusOr<T> to propagate a Status, or return a Handle<Value> with an Error
// value to introduce a CEL error.
//
// To introduce an extension function that may accept any kind of CEL value as
// an argument, the wrapped function should use a Value<Handle> parameter and
// check the type of the argument at evaluation time.
//
// Supported CEL to C++ type mappings:
// bool -> bool
// double -> double
// uint -> uint64_t
// int -> int64_t
// timestamp -> absl::Time
// duration -> absl::Duration
//
// Complex types may be referred to by cref or handle.
// To return these, users should return a Handle<Value>.
// any/dyn -> Handle<Value>, const Value&
// string -> Handle<StringValue> | const StringValue&
// bytes -> Handle<BytesValue> | const BytesValue&
// list -> Handle<ListValue> | const ListValue&
// map -> Handle<MapValue> | const MapValue&
// struct -> Handle<StructValue> | const StructValue&
// null -> Handle<NullValue> | const NullValue&
//
// To intercept error and unknown arguments, users must use a non-strict
// overload with all arguments typed as any and check the kind of the
// Handle<Value> argument.
//
// Example Usage:
//  double SquareDifference(ValueFactory&, double x, double y) {
//    return x * x - y * y;
//  }
//
//  {
//    std::unique_ptr<CelExpressionBuilder> builder;
//    // Initialize Expression builder with built-ins as needed.
//
//    CEL_RETURN_IF_ERROR(
//      builder->GetRegistry()->Register(
//        UnaryFunctionAdapter<double, double, double>::CreateDescriptor(
//          "sq_diff", /*receiver_style=*/false),
//        BinaryFunctionAdapter<double, double, double>::WrapFunction(
//          &SquareDifference)));
//  }
//
// example CEL expression:
//  sq_diff(4, 3) == 7 [true]
//
template <typename T, typename U, typename V>
class BinaryFunctionAdapter {
 public:
  using FunctionType = std::function<T(ValueFactory&, U, V)>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<BinaryFunctionImpl>(std::move(fn));
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict = true) {
    return FunctionDescriptor(
        name, receiver_style,
        {internal::AdaptedKind<U>(), internal::AdaptedKind<V>()}, is_strict);
  }

 private:
  class BinaryFunctionImpl : public cel::Function {
   public:
    explicit BinaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Handle<Value>> Invoke(
        const FunctionEvaluationContext& context,
        absl::Span<const Handle<Value>> args) const override {
      using Arg1Traits = internal::AdaptedTypeTraits<U>;
      using Arg2Traits = internal::AdaptedTypeTraits<V>;
      if (args.size() != 2) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for binary function");
      }
      typename Arg1Traits::AssignableType arg1;
      typename Arg2Traits::AssignableType arg2;
      CEL_RETURN_IF_ERROR(internal::HandleToAdaptedVisitor{args[0]}(&arg1));
      CEL_RETURN_IF_ERROR(internal::HandleToAdaptedVisitor{args[1]}(&arg2));

      T result = fn_(context.value_factory(), Arg1Traits::ToArg(arg1),
                     Arg2Traits::ToArg(arg2));

      return internal::AdaptedToHandleVisitor{context.value_factory()}(
          std::move(result));
    }

   private:
    BinaryFunctionAdapter::FunctionType fn_;
  };
};

// Adapter class for generating CEL extension functions from a one argument
// function.
//
// See documentation for Binary Function adapter for general recommendations.
//
// Example Usage:
//  double Invert(ValueFactory&, double x) {
//   return 1 / x;
//  }
//
//  {
//    std::unique_ptr<CelExpressionBuilder> builder;
//
//    CEL_RETURN_IF_ERROR(
//      builder->GetRegistry()->Register(
//        UnaryFunctionAdapter<double, double>::CreateDescriptor("inv",
//        /*receiver_style=*/false),
//         UnaryFunctionAdapter<double, double>::WrapFunction(&Invert)));
//  }
//  // example CEL expression
//  inv(4) == 1/4 [true]
template <typename T, typename U>
class UnaryFunctionAdapter {
 public:
  using FunctionType = std::function<T(ValueFactory&, U)>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<UnaryFunctionImpl>(std::move(fn));
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict = true) {
    return FunctionDescriptor(name, receiver_style,
                              {internal::AdaptedKind<U>()}, is_strict);
  }

 private:
  class UnaryFunctionImpl : public cel::Function {
   public:
    explicit UnaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Handle<Value>> Invoke(
        const FunctionEvaluationContext& context,
        absl::Span<const Handle<Value>> args) const override {
      using ArgTraits = internal::AdaptedTypeTraits<U>;
      if (args.size() != 1) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for unary function");
      }
      typename ArgTraits::AssignableType arg1;

      CEL_RETURN_IF_ERROR(internal::HandleToAdaptedVisitor{args[0]}(&arg1));

      T result = fn_(context.value_factory(), ArgTraits::ToArg(arg1));

      return internal::AdaptedToHandleVisitor{context.value_factory()}(
          std::move(result));
    }

   private:
    FunctionType fn_;
  };
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_FUNCTION_ADAPTER_H_
