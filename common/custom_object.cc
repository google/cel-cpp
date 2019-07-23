#include "common/custom_object.h"

namespace google {
namespace api {
namespace expr {
namespace common {

Value OpaqueObject::GetMember(absl::string_view name) const {
  return Value::FromError(
      internal::NoSuchMember(name, object_type().full_name()));
}

google::rpc::Status OpaqueObject::ForEach(
    const std::function<google::rpc::Status(absl::string_view, const Value&)>&
        call) const {
  return internal::OkStatus();
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
