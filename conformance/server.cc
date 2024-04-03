#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "google/api/expr/conformance/v1alpha1/conformance_service.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/eval.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/code.pb.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/value.h"
#include "conformance/value_conversion.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/transform_utility.h"
#include "extensions/protobuf/enum_adapter.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/protobuf/type_reflector.h"
#include "internal/status_macros.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/constant_folding.h"
#include "runtime/managed_value_factory.h"
#include "runtime/optional_types.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"


using ::cel::CreateStandardRuntimeBuilder;
using ::cel::Runtime;
using ::cel::RuntimeOptions;
using ::cel::conformance_internal::FromConformanceValue;
using ::cel::conformance_internal::ToConformanceValue;
using ::cel::extensions::ProtobufRuntimeAdapter;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel::extensions::RegisterProtobufEnum;

using ::google::protobuf::Arena;

ABSL_FLAG(bool, opt, false, "Enable optimizations (constant folding)");
ABSL_FLAG(
    bool, modern, false,
    "Use modern cel::Value APIs implementation of the conformance service.");
ABSL_FLAG(bool, arena, false,
          "Use arena memory manager (default: global heap ref-counted). Only "
          "affects the modern implementation");

namespace google::api::expr::runtime {

google::rpc::Code ToGrpcCode(absl::StatusCode code) {
  return static_cast<google::rpc::Code>(code);
}

class ConformanceServiceInterface {
 public:
  virtual ~ConformanceServiceInterface() = default;

  virtual void Parse(const conformance::v1alpha1::ParseRequest& request,
                     conformance::v1alpha1::ParseResponse& response) = 0;

  virtual void Check(const conformance::v1alpha1::CheckRequest& request,
                     conformance::v1alpha1::CheckResponse& response) = 0;

  virtual absl::Status Eval(const conformance::v1alpha1::EvalRequest& request,
                            conformance::v1alpha1::EvalResponse& response) = 0;
};

// Return a normalized raw expr for evaluation.
google::api::expr::v1alpha1::Expr ExtractExpr(
    const conformance::v1alpha1::EvalRequest& request) {
  const v1alpha1::Expr* expr = nullptr;

  // For now, discard type-check information if any.
  if (request.has_parsed_expr()) {
    expr = &request.parsed_expr().expr();
  } else if (request.has_checked_expr()) {
    expr = &request.checked_expr().expr();
  }
  google::api::expr::v1alpha1::Expr out;
  (out).MergeFrom(*expr);
  return out;
}

void LegacyParse(const conformance::v1alpha1::ParseRequest& request,
                 conformance::v1alpha1::ParseResponse& response,
                 bool enable_optional_syntax) {
  if (request.cel_source().empty()) {
    auto issue = response.add_issues();
    issue->set_message("No source code");
    issue->set_code(google::rpc::Code::INVALID_ARGUMENT);
    return;
  }
  cel::ParserOptions options;
  options.enable_optional_syntax = enable_optional_syntax;
  auto parse_status = parser::Parse(request.cel_source(), "", options);
  if (!parse_status.ok()) {
    auto issue = response.add_issues();
    *issue->mutable_message() = std::string(parse_status.status().message());
    issue->set_code(google::rpc::Code::INVALID_ARGUMENT);
    return;
  }
  google::api::expr::v1alpha1::ParsedExpr out;
  (out).MergeFrom(parse_status.value());
  *response.mutable_parsed_expr() = out;
}

class LegacyConformanceServiceImpl : public ConformanceServiceInterface {
 public:
  static absl::StatusOr<std::unique_ptr<LegacyConformanceServiceImpl>> Create(
      bool optimize) {
    static auto* constant_arena = new Arena();

    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::NestedTestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::NestedTestAllTypes>();

    InterpreterOptions options;
    options.enable_qualified_type_identifiers = true;
    options.enable_timestamp_duration_overflow_errors = true;
    options.enable_heterogeneous_equality = true;
    options.enable_empty_wrapper_null_unboxing = true;

    if (optimize) {
      std::cerr << "Enabling optimizations" << std::endl;
      options.constant_folding = true;
      options.constant_arena = constant_arena;
    }

    std::unique_ptr<CelExpressionBuilder> builder =
        CreateCelExpressionBuilder(options);
    auto type_registry = builder->GetTypeRegistry();
    type_registry->Register(
        google::api::expr::test::v1::proto2::GlobalEnum_descriptor());
    type_registry->Register(
        google::api::expr::test::v1::proto3::GlobalEnum_descriptor());
    type_registry->Register(google::api::expr::test::v1::proto2::TestAllTypes::
                                NestedEnum_descriptor());
    type_registry->Register(google::api::expr::test::v1::proto3::TestAllTypes::
                                NestedEnum_descriptor());
    CEL_RETURN_IF_ERROR(
        RegisterBuiltinFunctions(builder->GetRegistry(), options));

    return absl::WrapUnique(
        new LegacyConformanceServiceImpl(std::move(builder)));
  }

  void Parse(const conformance::v1alpha1::ParseRequest& request,
             conformance::v1alpha1::ParseResponse& response) override {
    LegacyParse(request, response, /*enable_optional_syntax=*/false);
  }

  void Check(const conformance::v1alpha1::CheckRequest& request,
             conformance::v1alpha1::CheckResponse& response) override {
    auto issue = response.add_issues();
    issue->set_message("Check is not supported");
    issue->set_code(google::rpc::Code::UNIMPLEMENTED);
  }

  absl::Status Eval(const conformance::v1alpha1::EvalRequest& request,
                    conformance::v1alpha1::EvalResponse& response) override {
    Arena arena;
    google::api::expr::v1alpha1::SourceInfo source_info;
    google::api::expr::v1alpha1::Expr expr = ExtractExpr(request);
    builder_->set_container(request.container());
    auto cel_expression_status =
        builder_->CreateExpression(&expr, &source_info);

    if (!cel_expression_status.ok()) {
      return absl::InternalError(cel_expression_status.status().ToString());
    }

    auto cel_expression = std::move(cel_expression_status.value());
    Activation activation;

    for (const auto& pair : request.bindings()) {
      auto* import_value = Arena::Create<google::api::expr::v1alpha1::Value>(&arena);
      (*import_value).MergeFrom(pair.second.value());
      auto import_status = ValueToCelValue(*import_value, &arena);
      if (!import_status.ok()) {
        return absl::InternalError(import_status.status().ToString());
      }
      activation.InsertValue(pair.first, import_status.value());
    }

    auto eval_status = cel_expression->Evaluate(activation, &arena);
    if (!eval_status.ok()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = eval_status.status().ToString();
      return absl::OkStatus();
    }

    CelValue result = eval_status.value();
    if (result.IsError()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(result.ErrorOrDie()->message());
    } else {
      google::api::expr::v1alpha1::Value export_value;
      auto export_status = CelValueToValue(result, &export_value);
      if (!export_status.ok()) {
        return absl::InternalError(export_status.ToString());
      }
      auto* result_value = response.mutable_result()->mutable_value();
      (*result_value).MergeFrom(export_value);
    }
    return absl::OkStatus();
  }

 private:
  explicit LegacyConformanceServiceImpl(
      std::unique_ptr<CelExpressionBuilder> builder)
      : builder_(std::move(builder)) {}

  std::unique_ptr<CelExpressionBuilder> builder_;
};

class ModernConformanceServiceImpl : public ConformanceServiceInterface {
 public:
  static absl::StatusOr<std::unique_ptr<ModernConformanceServiceImpl>> Create(
      bool optimize, bool use_arena) {
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::NestedTestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::NestedTestAllTypes>();

    RuntimeOptions options;
    options.enable_qualified_type_identifiers = true;
    options.enable_timestamp_duration_overflow_errors = true;
    options.enable_heterogeneous_equality = true;
    options.enable_empty_wrapper_null_unboxing = true;

    return absl::WrapUnique(
        new ModernConformanceServiceImpl(options, use_arena, optimize));
  }

  absl::StatusOr<std::unique_ptr<const cel::Runtime>> Setup(
      absl::string_view container) {
    RuntimeOptions options(options_);
    options.container = std::string(container);
    CEL_ASSIGN_OR_RETURN(auto builder, CreateStandardRuntimeBuilder(options));

    if (enable_optimizations_) {
      CEL_RETURN_IF_ERROR(cel::extensions::EnableConstantFolding(
          builder, constant_memory_manager_));
    }
    CEL_RETURN_IF_ERROR(cel::EnableReferenceResolver(
        builder, cel::ReferenceResolverEnabled::kAlways));

    auto& type_registry = builder.type_registry();
    // Use linked pbs in the
    type_registry.AddTypeProvider(
        std::make_unique<cel::extensions::ProtoTypeReflector>());
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry,
        google::api::expr::test::v1::proto2::GlobalEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry,
        google::api::expr::test::v1::proto3::GlobalEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry, google::api::expr::test::v1::proto2::TestAllTypes::
                           NestedEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry, google::api::expr::test::v1::proto3::TestAllTypes::
                           NestedEnum_descriptor()));

    CEL_RETURN_IF_ERROR(cel::extensions::EnableOptionalTypes(builder));

    return std::move(builder).Build();
  }

  void Parse(const conformance::v1alpha1::ParseRequest& request,
             conformance::v1alpha1::ParseResponse& response) override {
    LegacyParse(request, response, /*enable_optional_syntax=*/true);
  }

  void Check(const conformance::v1alpha1::CheckRequest& request,
             conformance::v1alpha1::CheckResponse& response) override {
    auto issue = response.add_issues();
    issue->set_message("Check is not supported");
    issue->set_code(google::rpc::Code::UNIMPLEMENTED);
  }

  absl::Status Eval(const conformance::v1alpha1::EvalRequest& request,
                    conformance::v1alpha1::EvalResponse& response) override {
    google::api::expr::v1alpha1::Expr expr = ExtractExpr(request);
    google::protobuf::Arena arena;
    auto proto_memory_manager = ProtoMemoryManagerRef(&arena);
    cel::MemoryManagerRef memory_manager =
        (use_arena_ ? proto_memory_manager
                    : cel::MemoryManagerRef::ReferenceCounting());

    auto runtime_status = Setup(request.container());
    if (!runtime_status.ok()) {
      return absl::InternalError(runtime_status.status().ToString(
          absl::StatusToStringMode::kWithEverything));
    }
    std::unique_ptr<const cel::Runtime> runtime =
        std::move(runtime_status).value();

    auto program_status = ProtobufRuntimeAdapter::CreateProgram(*runtime, expr);
    if (!program_status.ok()) {
      return absl::InternalError(program_status.status().ToString(
          absl::StatusToStringMode::kWithEverything));
    }
    std::unique_ptr<cel::TraceableProgram> program =
        std::move(program_status).value();
    cel::ManagedValueFactory value_factory(program->GetTypeProvider(),
                                           memory_manager);
    cel::Activation activation;

    for (const auto& pair : request.bindings()) {
      google::api::expr::v1alpha1::Value import_value;
      (import_value).MergeFrom(pair.second.value());
      auto import_status =
          FromConformanceValue(value_factory.get(), import_value);
      if (!import_status.ok()) {
        return absl::InternalError(import_status.status().ToString());
      }

      activation.InsertOrAssignValue(pair.first,
                                     std::move(import_status).value());
    }

    auto eval_status = program->Evaluate(activation, value_factory.get());
    if (!eval_status.ok()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = eval_status.status().ToString(
          absl::StatusToStringMode::kWithEverything);
      return absl::OkStatus();
    }

    cel::Value result = eval_status.value();
    if (result->Is<cel::ErrorValue>()) {
      const absl::Status& error = result->As<cel::ErrorValue>().NativeValue();
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(
          error.ToString(absl::StatusToStringMode::kWithEverything));
    } else {
      auto export_status = ToConformanceValue(value_factory.get(), result);
      if (!export_status.ok()) {
        return absl::InternalError(export_status.status().ToString(
            absl::StatusToStringMode::kWithEverything));
      }
      auto* result_value = response.mutable_result()->mutable_value();
      (*result_value).MergeFrom(*export_status);
    }
    return absl::OkStatus();
  }

 private:
  explicit ModernConformanceServiceImpl(const RuntimeOptions& options,
                                        bool use_arena,
                                        bool enable_optimizations)
      : options_(options),
        use_arena_(use_arena),
        enable_optimizations_(enable_optimizations),
        constant_memory_manager_(
            use_arena_ ? ProtoMemoryManagerRef(&constant_arena_)
                       : cel::MemoryManagerRef::ReferenceCounting()) {}

  RuntimeOptions options_;
  bool use_arena_;
  bool enable_optimizations_;
  Arena constant_arena_;
  cel::MemoryManagerRef constant_memory_manager_;
};

class PipeCodec {
 public:
  PipeCodec() = default;

  absl::Status Decode(const std::string& data, google::protobuf::Message* out) {
    std::string proto_bytes;
    if (!absl::Base64Unescape(data, &proto_bytes)) {
      return absl::InvalidArgumentError("invalid base64");
    }
    if (!out->ParseFromString(proto_bytes)) {
      return absl::InvalidArgumentError("invalid proto bytes");
    }
    return absl::OkStatus();
  }

  absl::Status Encode(const google::protobuf::Message& msg, std::string* out) {
    std::string data = msg.SerializeAsString();
    *out = absl::Base64Escape(data);
    return absl::OkStatus();
  }
};

int RunServer(bool optimize, bool modern, bool arena) {
  absl::StatusOr<std::unique_ptr<ConformanceServiceInterface>> service_or;
  if (modern) {
    service_or = ModernConformanceServiceImpl::Create(optimize, arena);
  } else {
    service_or = LegacyConformanceServiceImpl::Create(optimize);
  }

  if (!service_or.ok()) {
    std::cerr << "failed to create conformance service " << service_or.status()
              << std::endl;
    return 1;
  }

  auto conformance_service = std::move(service_or).value();

  PipeCodec pipe_codec;
  // Implementation of a simple pipe protocol:
  // INPUT LINE 1: parse/check/eval
  // INPUT LINE 2: base64 wire format of the corresponding request protobuf
  // OUTPUT LINE 1: base64 wire format of the corresponding response protobuf
  while (true) {
    std::string cmd, input, output;
    std::getline(std::cin, cmd);
    std::getline(std::cin, input);
    if (cmd == "parse") {
      conformance::v1alpha1::ParseRequest request;
      conformance::v1alpha1::ParseResponse response;
      if (!pipe_codec.Decode(input, &request).ok()) {
        std::cerr << "Failed to decode ParseRequest: " << std::endl;
      }
      conformance_service->Parse(request, response);
      auto status = pipe_codec.Encode(response, &output);
      if (!status.ok()) {
        std::cerr << "Failed to encode ParseResponse: " << status.ToString()
                  << std::endl;
      }
    } else if (cmd == "eval") {
      conformance::v1alpha1::EvalRequest request;
      conformance::v1alpha1::EvalResponse response;
      if (!pipe_codec.Decode(input, &request).ok()) {
        std::cerr << "Failed to decode EvalRequest" << std::endl;
      }
      auto status = conformance_service->Eval(request, response);
      if (!status.ok()) {
        std::cerr << status.ToString() << std::endl;
        auto* issue = response.add_issues();
        issue->set_message(status.message());
        issue->set_code(ToGrpcCode(status.code()));
      }
      status = pipe_codec.Encode(response, &output);
      if (!status.ok()) {
        std::cerr << "Failed to encode EvalResponse:" << status.ToString()
                  << std::endl;
      }
    } else if (cmd == "ping") {
      protobuf::Empty request, response;
      if (!pipe_codec.Decode(input, &request).ok()) {
        std::cerr << "Failed to decode PingRequest: " << std::endl;
      }
      auto status = pipe_codec.Encode(response, &output);
      if (!status.ok()) {
        std::cerr << "Failed to encode PingResponse: " << status.ToString()
                  << std::endl;
      }
    } else if (cmd.empty()) {
      return 0;
    } else {
      std::cerr << "Unexpected command: " << cmd << std::endl;
      return 2;
    }
    std::cout << output << std::endl;
  }

  return 0;
}

}  // namespace google::api::expr::runtime

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  return google::api::expr::runtime::RunServer(absl::GetFlag(FLAGS_opt),
                                               absl::GetFlag(FLAGS_modern),
                                               absl::GetFlag(FLAGS_arena));
}
