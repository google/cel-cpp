#include "internal/status_util.h"

#include "google/rpc/code.pb.h"
#include "absl/strings/str_cat.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

namespace {

google::rpc::Status NewStatus(google::rpc::Code code,
                              absl::string_view message) {
  google::rpc::Status error;
  error.set_code(code);
  error.set_message(std::string(message));
  return error;
}

}  // namespace

google::rpc::Status InvalidArgumentError(absl::string_view message) {
  return NewStatus(google::rpc::Code::INVALID_ARGUMENT, message);
}

google::rpc::Status NotFoundError(absl::string_view message) {
  return NewStatus(google::rpc::Code::NOT_FOUND, message);
}

google::rpc::Status UnimplementedError(absl::string_view message) {
  return NewStatus(google::rpc::Code::UNIMPLEMENTED, message);
}

google::rpc::Status OutOfRangeError(absl::string_view message) {
  return NewStatus(google::rpc::Code::OUT_OF_RANGE, message);
}

google::rpc::Status CancelledError() {
  return NewStatus(google::rpc::Code::CANCELLED, "");
}

google::rpc::Status InternalError(absl::string_view message) {
  return NewStatus(google::rpc::Code::INTERNAL, message);
}

google::rpc::Status OutOfRangeError(size_t index, size_t size) {
  return OutOfRangeError(absl::StrCat(index, " exceeds size ", size));
}

google::rpc::Status NoSuchCall(absl::string_view call,
                               absl::string_view full_type_name) {
  return NotFoundError(absl::StrCat(call, " not found in ", full_type_name));
}
google::rpc::Status NoSuchMember(absl::string_view member,
                                 absl::string_view full_type_name) {
  return NotFoundError(absl::StrCat(member, " not found in ", full_type_name));
}
google::rpc::Status UnknownType(absl::string_view full_type_name) {
  return InvalidArgumentError(absl::StrCat("Unknown type: ", full_type_name));
}
google::rpc::Status ParseError(absl::string_view full_type_name) {
  return InvalidArgumentError(absl::StrCat("Could not parse ", full_type_name));
}

google::rpc::Status UnexpectedType(absl::string_view full_type_name,
                                   absl::string_view context) {
  return InvalidArgumentError(
      absl::StrCat("Unexpected type, ", full_type_name, ", in ", context));
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
