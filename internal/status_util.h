#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_UTIL_H_

#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/strings/string_view.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

// Helper functions to create common error values.
google::rpc::Status InvalidArgumentError(absl::string_view message);
google::rpc::Status NotFoundError(absl::string_view message);
google::rpc::Status UnimplementedError(absl::string_view message);
google::rpc::Status OutOfRangeError(absl::string_view message);
google::rpc::Status InternalError(absl::string_view message);

google::rpc::Status CancelledError();

google::rpc::Status OutOfRangeError(size_t index, size_t size);
google::rpc::Status NoSuchCall(absl::string_view call,
                               absl::string_view full_type_name);
google::rpc::Status NoSuchMember(absl::string_view member,
                                 absl::string_view full_type_name);
google::rpc::Status UnexpectedType(absl::string_view full_type_name,
                                   absl::string_view context);

inline google::rpc::Status NoSuchKey(absl::string_view key_as_string) {
  return NoSuchMember(key_as_string, "map");
}

google::rpc::Status UnknownType(absl::string_view full_type_name);
google::rpc::Status ParseError(absl::string_view full_type_name);

inline google::rpc::Status OkStatus() { return google::rpc::Status(); }
inline bool IsOk(const google::rpc::Status& status) {
  return status.code() == google::rpc::Code::OK;
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_UTIL_H_
