#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/conformance_service.grpc.pb.h"
#include "google/api/expr/v1alpha1/eval.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/code.pb.h"
#include "grpcpp/grpcpp.h"
#include "absl/strings/str_split.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/transform_utility.h"
#include "internal/proto_util.h"
#include "parser/parser.h"
#include "absl/status/statusor.h"


using ::grpc::Status;
using ::grpc::StatusCode;
using ::google::protobuf::Arena;

namespace google {
namespace api {
namespace expr {
namespace runtime {

class ConformanceServiceImpl final
    : public v1alpha1::ConformanceService::Service {
 public:
  ConformanceServiceImpl(std::unique_ptr<CelExpressionBuilder> builder)
      : builder_(std::move(builder)) {}
  Status Parse(grpc::ServerContext* context,
               const v1alpha1::ParseRequest* request,
               v1alpha1::ParseResponse* response) override {
    if (request->cel_source().empty()) {
      return Status(StatusCode::INVALID_ARGUMENT, "No source code.");
    }
    auto parse_status = parser::Parse(request->cel_source(), "");
    if (!parse_status.ok()) {
      auto issue = response->add_issues();
      *issue->mutable_message() = std::string(parse_status.status().message());
      issue->set_code(google::rpc::Code::INVALID_ARGUMENT);
    } else {
      google::api::expr::v1alpha1::ParsedExpr out;
      (out).MergeFrom(parse_status.value());
      response->mutable_parsed_expr()->CopyFrom(out);
    }
    return Status::OK;
  }
  Status Check(grpc::ServerContext* context,
               const v1alpha1::CheckRequest* request,
               v1alpha1::CheckResponse* response) override {
    return Status(StatusCode::UNIMPLEMENTED, "Check is not supported");
  }
  Status Eval(grpc::ServerContext* context,
              const v1alpha1::EvalRequest* request,
              v1alpha1::EvalResponse* response) override {
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
    auto cel_expression_status = builder_->CreateExpression(&out, &source_info);

    if (!cel_expression_status.ok()) {
      return Status(StatusCode::INTERNAL,
                    std::string(cel_expression_status.status().message()));
    }

    auto cel_expression = std::move(cel_expression_status.value());
    Activation activation;

    for (const auto& pair : request->bindings()) {
      auto* import_value =
          Arena::CreateMessage<google::api::expr::v1alpha1::Value>(&arena);
      (*import_value).MergeFrom(pair.second.value());
      auto import_status = ValueToCelValue(*import_value, &arena);
      if (!import_status.ok()) {
        return Status(StatusCode::INTERNAL, import_status.status().ToString());
      }
      activation.InsertValue(pair.first, import_status.value());
    }

    auto eval_status = cel_expression->Evaluate(activation, &arena);
    if (!eval_status.ok()) {
      return Status(StatusCode::INTERNAL,
                    std::string(eval_status.status().message()));
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
        return Status(StatusCode::INTERNAL, export_status.ToString());
      }
      auto* result_value = response->mutable_result()->mutable_value();
      (*result_value).MergeFrom(export_value);
    }
    return Status::OK;
  }

 private:
  std::unique_ptr<CelExpressionBuilder> builder_;
};

int RunServer(std::string server_address) {
  google::protobuf::Arena arena;
  InterpreterOptions options;

  const char* enable_constant_folding =
      getenv("CEL_CPP_ENABLE_CONSTANT_FOLDING");
  if (enable_constant_folding != nullptr) {
    options.constant_folding = true;
    options.constant_arena = &arena;
  }

  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  auto register_status = RegisterBuiltinFunctions(builder->GetRegistry());
  if (!register_status.ok()) {
    return 1;
  }

  ConformanceServiceImpl service(std::move(builder));
  grpc::ServerBuilder grpc_builder;
  int port;
  grpc_builder.AddListeningPort(server_address,
                                grpc::InsecureServerCredentials(), &port);
  grpc_builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(grpc_builder.BuildAndStart());
  std::cout << "Listening on 127.0.0.1:" << port << std::endl;
  fflush(stdout);
  server->Wait();
  return 0;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

int main(int argc, char** argv) {
  std::string server_address = "127.0.0.1:0";
  return google::api::expr::runtime::RunServer(server_address);
}
