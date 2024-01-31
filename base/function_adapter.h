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
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "base/internal/function_adapter.h"
#include "base/kind.h"
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
  using AssignableType = T;

  static T ToArg(AssignableType v) { return v; }
};

template <typename... Args>
struct KindAdderImpl;

template <typename Arg, typename... Args>
struct KindAdderImpl<Arg, Args...> {
  static void AddTo(std::vector<cel::Kind>& args) {
    args.push_back(AdaptedKind<Arg>());
    KindAdderImpl<Args...>::AddTo(args);
  }
};

template <>
struct KindAdderImpl<> {
  static void AddTo(std::vector<cel::Kind>& args) {}
};

template <typename... Args>
struct KindAdder {
  static std::vector<cel::Kind> Kinds() {
    std::vector<cel::Kind> args;
    KindAdderImpl<Args...>::AddTo(args);
    return args;
  }
};

template <typename T>
struct ApplyReturnType {
  using type = absl::StatusOr<T>;
};

template <typename T>
struct ApplyReturnType<absl::StatusOr<T>> {
  using type = absl::StatusOr<T>;
};

template <int N, typename Arg, typename... Args>
struct IndexerImpl {
  using type = typename IndexerImpl<N - 1, Args...>::type;
};

template <typename Arg, typename... Args>
struct IndexerImpl<0, Arg, Args...> {
  using type = Arg;
};

template <int N, typename... Args>
struct Indexer {
  static_assert(N < sizeof...(Args) && N >= 0);
  using type = typename IndexerImpl<N, Args...>::type;
};

template <int N, typename... Args>
struct ApplyHelper {
  template <typename T, typename Op>
  static typename ApplyReturnType<T>::type Apply(
      Op&& op, absl::Span<const Value> input) {
    constexpr int idx = sizeof...(Args) - N;
    using Arg = typename Indexer<idx, Args...>::type;
    using ArgTraits = internal::AdaptedTypeTraits<Arg>;
    typename ArgTraits::AssignableType arg_i;
    CEL_RETURN_IF_ERROR(internal::HandleToAdaptedVisitor{input[idx]}(&arg_i));

    return ApplyHelper<N - 1, Args...>::template Apply<T>(
        absl::bind_front(std::forward<Op>(op), ArgTraits::ToArg(arg_i)), input);
  }
};

template <typename... Args>
struct ApplyHelper<0, Args...> {
  template <typename T, typename Op>
  static typename ApplyReturnType<T>::type Apply(
      Op&& op, absl::Span<const Value> input) {
    return op();
  }
};

}  // namespace internal

// Adapter class for generating CEL extension functions from a two argument
// function. Generates an implementation of the cel::Function interface that
// calls the function to wrap.
//
// Extension functions must distinguish between recoverable errors (error that
// should participate in CEL's error pruning) and unrecoverable errors (a non-ok
// absl::Status that stops evaluation). The function to wrap may return
// StatusOr<T> to propagate a Status, or return a Value with an Error
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
// To return these, users should return a Value.
// any/dyn -> Value, const Value&
// string -> StringValue | const StringValue&
// bytes -> BytesValue | const BytesValue&
// list -> ListValue | const ListValue&
// map -> MapValue | const MapValue&
// struct -> StructValue | const StructValue&
// null -> NullValue | const NullValue&
//
// To intercept error and unknown arguments, users must use a non-strict
// overload with all arguments typed as any and check the kind of the
// Value argument.
//
// Example Usage:
//  double SquareDifference(ValueManager&, double x, double y) {
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
  using FunctionType = std::function<T(ValueManager&, U, V)>;

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
    absl::StatusOr<Value> Invoke(const FunctionEvaluationContext& context,
                                 absl::Span<const Value> args) const override {
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
//  double Invert(ValueManager&, double x) {
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
  using FunctionType = std::function<T(ValueManager&, U)>;

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
    absl::StatusOr<Value> Invoke(const FunctionEvaluationContext& context,
                                 absl::Span<const Value> args) const override {
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

// Generic adapter class for generating CEL extension functions from an
// n-argument function. Prefer using the Binary and Unary versions. They are
// simpler and cover most use cases.
//
// See documentation for Binary Function adapter for general recommendations.
template <typename T, typename... Args>
class VariadicFunctionAdapter {
 public:
  using FunctionType = std::function<T(ValueManager&, Args...)>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<VariadicFunctionImpl>(std::move(fn));
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict = true) {
    return FunctionDescriptor(name, receiver_style,
                              internal::KindAdder<Args...>::Kinds(), is_strict);
  }

 private:
  class VariadicFunctionImpl : public cel::Function {
   public:
    explicit VariadicFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}

    absl::StatusOr<Value> Invoke(const FunctionEvaluationContext& context,
                                 absl::Span<const Value> args) const override {
      if (args.size() != sizeof...(Args)) {
        return absl::InvalidArgumentError(
            absl::StrCat("unexpected number of arguments for variadic(",
                         sizeof...(Args), ") function"));
      }

      CEL_ASSIGN_OR_RETURN(
          T result,
          (internal::ApplyHelper<sizeof...(Args), Args...>::template Apply<T>(
              absl::bind_front(fn_, std::ref(context.value_factory())), args)));
      return internal::AdaptedToHandleVisitor{context.value_factory()}(
          std::move(result));
    }

   private:
    FunctionType fn_;
  };
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_FUNCTION_ADAPTER_H_
