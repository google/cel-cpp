#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_STATUS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_STATUS_H_

#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/strings/string_view.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace util {

using Status = google::rpc::Status;

inline google::rpc::Status MakeStatus(google::rpc::Code code, std::string msg) {
  google::rpc::Status status;
  status.set_code(code);
  status.set_message(std::move(msg));
  return status;
}

inline google::rpc::Status OkStatus() {
  google::rpc::Status ok;
  ok.set_code(google::rpc::Code::OK);
  return ok;
}

inline bool IsOk(google::rpc::Status status) {
  return status.code() == google::rpc::Code::OK;
}

}  // namespace util
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_STATUS_H_
