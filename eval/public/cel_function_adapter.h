#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
#include <functional>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "base/statusor.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace internal {

// TypeCodeMatch template function family
// Used for CEL type deduction based on C++ native
// type.
template <class T>
absl::optional<CelValue::Type> TypeCodeMatch() {
  int index = CelValue::IndexOf<T>::value;
  if (index < 0) return {};
  CelValue::Type arg_type = static_cast<CelValue::Type>(index);
  if (arg_type >= CelValue::Type::kAny) {
    return {};
  }
  return arg_type;
}

// A bit of a trick - to pass Any kind of value, we use generic
// CelValue parameters.
template <>
absl::optional<CelValue::Type> TypeCodeMatch<CelValue>();

template <int N>
bool AddType(std::vector<CelValue::Type>*) {
  return true;
}

// AddType template method
// Appends CEL type constant deduced from C++ type Type to descriptor
template <int N, typename Type, typename... Args>
bool AddType(std::vector<CelValue::Type>* arg_types) {
  auto kind = TypeCodeMatch<Type>();
  if (!kind) {
    return false;
  }

  arg_types->push_back(kind.value());

  return AddType<N, Args...>(arg_types);

  return true;
}

}  // namespace internal

// FunctionAdapter is a helper class that simplifies creation of CelFunction
// implementations.
// It accepts method implementations as std::function, allowing
// them to be lambdas/regular C++ functions. CEL method descriptors are
// deduced based on C++ function signatures.
//
// Usage example:
//
//  auto func = [](google::protobuf::google::protobuf::Arena* arena, int64_t i, int64_t j) -> bool {
//    return i < j;
//  };
//
//  auto func_status =
//      FunctionAdapter<bool, int64_t, int64_t>::Create("<", false, func);
//
//  if(func_status.ok()) {
//     auto func = func_status.value();
//  }
template <typename ReturnType, typename... Arguments>
class FunctionAdapter : public CelFunction {
 public:
  using FuncType = std::function<ReturnType(::google::protobuf::Arena*, Arguments...)>;

  FunctionAdapter(const CelFunctionDescriptor& descriptor, FuncType handler)
      : CelFunction(descriptor), handler_(std::move(handler)) {}

  static cel_base::StatusOr<std::unique_ptr<CelFunction>> Create(
      absl::string_view name, bool receiver_type,
      std::function<ReturnType(::google::protobuf::Arena*, Arguments...)> handler) {
    std::vector<CelValue::Type> arg_types;

    if (!internal::AddType<0, Arguments...>(&arg_types)) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrCat("Failed to create adapter for ", name,
                       ": failed to determine input parameter type"));
    }

    std::unique_ptr<CelFunction> cel_func = absl::make_unique<FunctionAdapter>(
        CelFunctionDescriptor(std::string(name), receiver_type, arg_types),
        std::move(handler));
    return std::move(cel_func);
  }

  // Creates function handler and attempts to register it with
  // supplied function registry.
  static absl::Status CreateAndRegister(
      absl::string_view name, bool receiver_type,
      std::function<ReturnType(::google::protobuf::Arena*, Arguments...)> handler,
      CelFunctionRegistry* registry) {
    auto status = Create(name, receiver_type, std::move(handler));
    if (!status.ok()) {
      return status.status();
    }

    return registry->Register(std::move(status.value()));
  }

#if defined(__clang_major_version__) && __clang_major_version__ >= 8 && !defined(__APPLE__)
  template <int arg_index>
  inline absl::Status RunWrap(absl::Span<const CelValue> arguments,
                              std::tuple<::google::protobuf::Arena*, Arguments...> input,
                              CelValue* result, ::google::protobuf::Arena* arena) const {
    if (!ConvertFromValue(arguments[arg_index],
                          &std::get<arg_index + 1>(input))) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Type conversion failed");
    }
    return RunWrap<arg_index + 1>(arguments, input, result, arena);
  }

  template <>
  inline absl::Status RunWrap<sizeof...(Arguments)>(
      absl::Span<const CelValue>,
      std::tuple<::google::protobuf::Arena*, Arguments...> input, CelValue* result,
      ::google::protobuf::Arena* arena) const {
    return CreateReturnValue(absl::apply(handler_, input), arena, result);
  }
#else
  inline absl::Status RunWrap(std::function<ReturnType()> func,
                              const absl::Span<const CelValue> argset,
                              ::google::protobuf::Arena* arena, CelValue* result,
                              int arg_index) const {
    return CreateReturnValue(func(), arena, result);
  }

  template <typename Arg, typename... Args>
  inline absl::Status RunWrap(std::function<ReturnType(Arg, Args...)> func,
                              const absl::Span<const CelValue> argset,
                              ::google::protobuf::Arena* arena, CelValue* result,
                              int arg_index) const {
    Arg argument;
    if (!ConvertFromValue(argset[arg_index], &argument)) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Type conversion failed");
    }

    std::function<ReturnType(Args...)> wrapped_func =
        [func, argument](Args... args) -> ReturnType {
      return func(argument, args...);
    };

    return RunWrap(std::move(wrapped_func), argset, arena, result,
                   arg_index + 1);
  }
#endif

  absl::Status Evaluate(absl::Span<const CelValue> arguments, CelValue* result,
                        ::google::protobuf::Arena* arena) const override {
    if (arguments.size() != sizeof...(Arguments)) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Argument number mismatch");
    }

#if defined(__clang_major_version__) && __clang_major_version__ >= 8 && !defined(__APPLE__)
    std::tuple<::google::protobuf::Arena*, Arguments...> input;
    std::get<0>(input) = arena;
    return RunWrap<0>(arguments, input, result, arena);
#else
    const auto* handler = &handler_;
    std::function<ReturnType(Arguments...)> wrapped_handler =
        [handler, arena](Arguments... args) -> ReturnType {
      return (*handler)(arena, args...);
    };
    return RunWrap(std::move(wrapped_handler), arguments, arena, result, 0);
#endif
  }

 private:
  template <class ArgType>
  static bool ConvertFromValue(CelValue value, ArgType* result) {
    return value.GetValue(result);
  }

  // Special conversion - from CelValue to CelValue - plain copy
  static bool ConvertFromValue(CelValue value, CelValue* result) {
    *result = std::move(value);
    return true;
  }

  // CreateReturnValue method wraps evaluation result with CelValue.
  static absl::Status CreateReturnValue(bool value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    *result = CelValue::CreateBool(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(int64_t value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    *result = CelValue::CreateInt64(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(uint64_t value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    *result = CelValue::CreateUint64(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(double value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    *result = CelValue::CreateDouble(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(CelValue::StringHolder value,
                                        ::google::protobuf::Arena*, CelValue* result) {
    *result = CelValue::CreateString(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(CelValue::BytesHolder value,
                                        ::google::protobuf::Arena*, CelValue* result) {
    *result = CelValue::CreateBytes(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(const ::google::protobuf::Message* value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null Message pointer returned");
    }
    *result = CelValue::CreateMessage(value, arena);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(const CelList* value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null CelList pointer returned");
    }
    *result = CelValue::CreateList(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(const CelMap* value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null CelMap pointer returned");
    }
    *result = CelValue::CreateMap(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(const CelError* value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null CelError pointer returned");
    }
    *result = CelValue::CreateError(value);
    return absl::OkStatus();
  }

  static absl::Status CreateReturnValue(const CelValue& value, ::google::protobuf::Arena*,
                                        CelValue* result) {
    *result = value;
    return absl::OkStatus();
  }

  template <typename T>
  static absl::Status CreateReturnValue(const cel_base::StatusOr<T>& value,
                                        ::google::protobuf::Arena*, CelValue*) {
    if (!value) {
      return value.status();
    }
    return CreateReturnValue(value.value());
  }

  FuncType handler_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
