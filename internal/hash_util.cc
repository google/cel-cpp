#include "internal/hash_util.h"

#include <limits>

namespace google {
namespace api {
namespace expr {
namespace internal {

std::size_t HashImpl(const std::string& value, specialize) {
  return StdHash(value);
}

std::size_t HashImpl(const google::rpc::Status& value, specialize) {
  std::size_t hash = Hash(value.code());
  AccumulateHash(value.message(), &hash);
  return hash;
}

std::size_t HashImpl(absl::string_view value, specialize) {
  return StdHash(std::string(value));
}

std::size_t HashImpl(absl::Duration value, specialize) {
  return StdHash(absl::ToInt64Nanoseconds(value));
}

std::size_t HashImpl(absl::Time value, specialize) {
  return StdHash(absl::ToUnixNanos(value));
}

std::size_t HashImpl(std::nullptr_t, specialize) { return 0; }

std::size_t HashImpl(const google::protobuf::Any& value, specialize) {
  return Hash(value.type_url(), value.value());
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
