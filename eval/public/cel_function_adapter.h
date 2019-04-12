#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
#include <functional>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/strings/str_cat.h"
#include "eval/proto/cel_error.pb.h"
#include "eval/public/cel_function.h"
#include "google/rpc/status.pb.h"
#include "eval/public/cel_status_or.h"

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
bool AddType(CelFunction::Descriptor* descriptor) {
  return true;
}

// AddType template method
// Appends CEL type constant deduced from C++ type Type to descriptor
template <int N, typename Type, typename... Args>
bool AddType(CelFunction::Descriptor* descriptor) {
  auto kind = TypeCodeMatch<Type>();
  if (!kind) {
    return false;
  }

  descriptor->types.push_back(kind.value());

  return AddType<N, Args...>(descriptor);

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
//  if(util::IsOk(func_status)) {
//     auto func = func_status.ValueOrDie();
//  }
template <typename ReturnType, typename... Arguments>
class FunctionAdapter : public CelFunction {
 public:
  using FuncType = std::function<ReturnType(::google::protobuf::Arena*, Arguments...)>;

  FunctionAdapter(const Descriptor& descriptor, FuncType handler)
      : CelFunction(descriptor), handler_(std::move(handler)) {
  }

  static util::StatusOr<std::unique_ptr<CelFunction>> Create(
      absl::string_view name, bool receiver_type,
      std::function<ReturnType(::google::protobuf::Arena*, Arguments...)> handler) {
    CelFunction::Descriptor descriptor;
    descriptor.name = std::string(name);
    descriptor.receiver_style = receiver_type;

    if (!internal::AddType<0, Arguments...>(&descriptor)) {
      return util::MakeStatus(
          google::rpc::Code::INTERNAL,
          absl::StrCat("Failed to create adapter for ", name,
                       ": failed to determine input parameter type"));
    }

    std::unique_ptr<CelFunction> cel_func =
        absl::make_unique<FunctionAdapter>(descriptor, std::move(handler));
    return std::move(cel_func);
  }

  // Creates function handler and attempts to register it with
  // supplied function registry.
  static util::Status CreateAndRegister(
      absl::string_view name, bool receiver_type,
      std::function<ReturnType(::google::protobuf::Arena*, Arguments...)> handler,
      CelFunctionRegistry* registry) {
    auto status = Create(name, receiver_type, std::move(handler));
    if (!util::IsOk(status)) {
      return status.status();
    }

    return registry->Register(std::move(status.ValueOrDie()));
  }

  util::Status RunWrap(std::function<ReturnType()> func,
                       const absl::Span<const CelValue> argset,
                       ::google::protobuf::Arena* arena, CelValue* result,
                       int arg_index) const {
    return CreateReturnValue(func(), arena, result);
  }

  template <typename Arg, typename... Args>
  util::Status RunWrap(std::function<ReturnType(Arg, Args...)> func,
                       const absl::Span<const CelValue> argset,
                       ::google::protobuf::Arena* arena, CelValue* result,
                       int arg_index) const {
    Arg argument;
    if (!ConvertFromValue(argset[arg_index], &argument)) {
      return util::MakeStatus(google::rpc::Code::INVALID_ARGUMENT,
                          "Type conversion failed");
    }

    std::function<ReturnType(Args...)> wrapped_func =
        [func, argument](Args... args) -> ReturnType {
      return func(argument, args...);
    };

    return RunWrap(std::move(wrapped_func), argset, arena, result,
                   arg_index + 1);
  }

  util::Status Evaluate(absl::Span<const CelValue> arguments,
                          CelValue* result,
                          ::google::protobuf::Arena* arena) const override {
    if (arguments.size() != sizeof...(Arguments)) {
      return util::MakeStatus(google::rpc::Code::INTERNAL,
                          "Argument number mismatch");
    }

    const auto* handler = &handler_;
    // Replace witj bind_front(std::cref(handler_), arena), when it is
    // available.
    std::function<ReturnType(Arguments...)> wrapped_handler =
        [handler, arena](Arguments... args) -> ReturnType {
      return (*handler)(arena, args...);
    };

    return RunWrap(std::move(wrapped_handler), arguments, arena, result, 0);
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
  static util::Status CreateReturnValue(bool value, ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    *result = CelValue::CreateBool(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(int64_t value, ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    *result = CelValue::CreateInt64(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(uint64_t value, ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    *result = CelValue::CreateUint64(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(double value, ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    *result = CelValue::CreateDouble(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(CelValue::StringHolder value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    *result = CelValue::CreateString(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(CelValue::BytesHolder value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    *result = CelValue::CreateBytes(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(const ::google::protobuf::Message* value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    if (value == nullptr) {
      return util::MakeStatus(google::rpc::Code::INVALID_ARGUMENT,
                          "Null Message pointer returned");
    }
    *result = CelValue::CreateMessage(value, arena);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(const CelList* value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    if (value == nullptr) {
      return util::MakeStatus(google::rpc::Code::INVALID_ARGUMENT,
                          "Null CelList pointer returned");
    }
    *result = CelValue::CreateList(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(const CelMap* value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    if (value == nullptr) {
      return util::MakeStatus(google::rpc::Code::INVALID_ARGUMENT,
                          "Null CelMap pointer returned");
    }
    *result = CelValue::CreateMap(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(const CelError* value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    if (value == nullptr) {
      return util::MakeStatus(google::rpc::Code::INVALID_ARGUMENT,
                          "Null CelError pointer returned");
    }
    *result = CelValue::CreateError(value);
    return util::OkStatus();
  }

  static util::Status CreateReturnValue(const CelValue& value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    *result = value;
    return util::OkStatus();
  }

  template <typename T>
  static util::Status CreateReturnValue(const util::StatusOr<T>& value,
                                        ::google::protobuf::Arena* arena,
                                        CelValue* result) {
    if (!value) {
      return value.status();
    }
    return CreateReturnValue(value.ValueOrDie());
  }

  FuncType handler_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
