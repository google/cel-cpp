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
// TODO(issues/5): Add generalized version in addition to the common cases
// of unary/binary functions.

#ifndef THIRD_PARTY_CEL_CPP_BASE_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_BASE_FUNCTION_ADAPTER_H_

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "base/function.h"
#include "base/function_interface.h"
#include "base/internal/function_adapter.h"

namespace cel {

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
// double -> double
// uint -> uint64_t
// int -> int64_t
// dyn -> Handle<Value>
// TODO(issues/5): add support for remaining builtin types.
//
// Example Usage:
//  double SquareDifference(double x, double y) {
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
      if (args.size() != 2) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for binary function");
      }
      U arg1;
      V arg2;
      CEL_RETURN_IF_ERROR(internal::HandleToAdaptedVisitor{args[0]}(&arg1));
      CEL_RETURN_IF_ERROR(internal::HandleToAdaptedVisitor{args[1]}(&arg2));

      T result = fn_(context.value_factory(), arg1, arg2);

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
//  double Invert(double x) {
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
      if (args.size() != 1) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for unary function");
      }
      U arg1;

      CEL_RETURN_IF_ERROR(internal::HandleToAdaptedVisitor{args[0]}(&arg1));

      T result = fn_(context.value_factory(), arg1);

      return internal::AdaptedToHandleVisitor{context.value_factory()}(
          std::move(result));
    }

   private:
    FunctionType fn_;
  };
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_FUNCTION_ADAPTER_H_
