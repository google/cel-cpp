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
#include "google/rpc/status.pb.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "conformance/service.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

ABSL_FLAG(bool, opt, false, "Enable optimizations (constant folding)");
ABSL_FLAG(
    bool, modern, false,
    "Use modern cel::Value APIs implementation of the conformance service.");
ABSL_FLAG(bool, arena, false,
          "Use arena memory manager (default: global heap ref-counted). Only "
          "affects the modern implementation");
ABSL_FLAG(bool, recursive, false,
          "Enable recursive plans. Depth limited to slightly more than the "
          "default nesting limit.");

namespace google::api::expr::runtime {

google::rpc::Code ToGrpcCode(absl::StatusCode code) {
  return static_cast<google::rpc::Code>(code);
}

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

int RunServer(bool optimize, bool modern, bool arena, bool recursive) {
  absl::StatusOr<std::unique_ptr<cel_conformance::ConformanceServiceInterface>>
      service_or = cel_conformance::NewConformanceService(
          cel_conformance::ConformanceServiceOptions{.optimize = optimize,
                                                     .modern = modern,
                                                     .arena = arena,
                                                     .recursive = recursive});

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
  return google::api::expr::runtime::RunServer(
      absl::GetFlag(FLAGS_opt), absl::GetFlag(FLAGS_modern),
      absl::GetFlag(FLAGS_arena), absl::GetFlag(FLAGS_recursive));
}
