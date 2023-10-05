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
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/transform_utility.h"
#include "internal/status_macros.h"
#include "parser/parser.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"


using ::google::protobuf::Arena;

ABSL_FLAG(bool, opt, false, "Enable optimizations (constant folding)");

namespace google::api::expr::runtime {

class ConformanceServiceInterface {
 public:
  virtual ~ConformanceServiceInterface() = default;

  virtual void Parse(const conformance::v1alpha1::ParseRequest* request,
                     conformance::v1alpha1::ParseResponse* response) = 0;

  virtual void Check(const conformance::v1alpha1::CheckRequest* request,
                     conformance::v1alpha1::CheckResponse* response) = 0;

  virtual void Eval(const conformance::v1alpha1::EvalRequest* request,
                    conformance::v1alpha1::EvalResponse* response) = 0;
};

class ConformanceServiceImpl : public ConformanceServiceInterface {
 public:
  static absl::StatusOr<std::unique_ptr<ConformanceServiceImpl>> Create(
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

    return absl::WrapUnique(new ConformanceServiceImpl(std::move(builder)));
  }

  void Parse(const conformance::v1alpha1::ParseRequest* request,
             conformance::v1alpha1::ParseResponse* response) override {
    if (request->cel_source().empty()) {
      auto issue = response->add_issues();
      issue->set_message("No source code");
      issue->set_code(google::rpc::Code::INVALID_ARGUMENT);
      return;
    }
    auto parse_status = parser::Parse(request->cel_source(), "");
    if (!parse_status.ok()) {
      auto issue = response->add_issues();
      *issue->mutable_message() = std::string(parse_status.status().message());
      issue->set_code(google::rpc::Code::INVALID_ARGUMENT);
    } else {
      google::api::expr::v1alpha1::ParsedExpr out;
      (out).MergeFrom(parse_status.value());
      *response->mutable_parsed_expr() = out;
    }
  }

  void Check(const conformance::v1alpha1::CheckRequest* request,
             conformance::v1alpha1::CheckResponse* response) override {
    auto issue = response->add_issues();
    issue->set_message("Check is not supported");
    issue->set_code(google::rpc::Code::UNIMPLEMENTED);
  }

  void Eval(const conformance::v1alpha1::EvalRequest* request,
            conformance::v1alpha1::EvalResponse* response) override {
    const v1alpha1::Expr* expr = nullptr;
    if (request->has_parsed_expr()) {
      expr = &request->parsed_expr().expr();
    } else if (request->has_checked_expr()) {
      expr = &request->checked_expr().expr();
    }

    Arena arena;
    google::api::expr::v1alpha1::SourceInfo source_info;
    google::api::expr::v1alpha1::Expr out;
    (out).MergeFrom(*expr);
    builder_->set_container(request->container());
    auto cel_expression_status = builder_->CreateExpression(&out, &source_info);

    if (!cel_expression_status.ok()) {
      auto issue = response->add_issues();
      issue->set_message(cel_expression_status.status().ToString());
      issue->set_code(google::rpc::Code::INTERNAL);
      return;
    }

    auto cel_expression = std::move(cel_expression_status.value());
    Activation activation;

    for (const auto& pair : request->bindings()) {
      auto* import_value =
          Arena::CreateMessage<google::api::expr::v1alpha1::Value>(&arena);
      (*import_value).MergeFrom(pair.second.value());
      auto import_status = ValueToCelValue(*import_value, &arena);
      if (!import_status.ok()) {
        auto issue = response->add_issues();
        issue->set_message(import_status.status().ToString());
        issue->set_code(google::rpc::Code::INTERNAL);
        return;
      }
      activation.InsertValue(pair.first, import_status.value());
    }

    auto eval_status = cel_expression->Evaluate(activation, &arena);
    if (!eval_status.ok()) {
      *response->mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = eval_status.status().ToString();
      return;
    }

    CelValue result = eval_status.value();
    if (result.IsError()) {
      *response->mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(result.ErrorOrDie()->message());
    } else {
      google::api::expr::v1alpha1::Value export_value;
      auto export_status = CelValueToValue(result, &export_value);
      if (!export_status.ok()) {
        auto issue = response->add_issues();
        issue->set_message(export_status.ToString());
        issue->set_code(google::rpc::Code::INTERNAL);
        return;
      }
      auto* result_value = response->mutable_result()->mutable_value();
      (*result_value).MergeFrom(export_value);
    }
  }

 private:
  explicit ConformanceServiceImpl(std::unique_ptr<CelExpressionBuilder> builder)
      : builder_(std::move(builder)) {}

  std::unique_ptr<CelExpressionBuilder> builder_;
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

int RunServer(bool optimize) {
  absl::StatusOr<std::unique_ptr<ConformanceServiceInterface>> service_or =
      ConformanceServiceImpl::Create(optimize);

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
      conformance_service->Parse(&request, &response);
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
      conformance_service->Eval(&request, &response);
      auto status = pipe_codec.Encode(response, &output);
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
  return google::api::expr::runtime::RunServer(absl::GetFlag(FLAGS_opt));
}
