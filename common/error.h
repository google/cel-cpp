#ifndef THIRD_PARTY_CEL_CPP_COMMON_ERROR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ERROR_H_

#include "google/rpc/status.pb.h"
#include "absl/container/node_hash_set.h"
#include "absl/types/span.h"
#include "internal/hash_util.h"
#include "internal/proto_util.h"

namespace google {
namespace api {
namespace expr {
namespace common {

/** A CEL Error. */
class Error {
 public:
  using ErrorData = absl::node_hash_set<google::rpc::Status, internal::Hasher,
                                        internal::DefaultProtoEqual>;

  explicit Error(const google::rpc::Status& error);
  explicit Error(absl::Span<const google::rpc::Status* const> errors);
  explicit Error(absl::Span<const google::rpc::Status> errors);

  const ErrorData& errors() const;

  bool operator==(const Error& rhs) const;
  inline bool operator!=(const Error& rhs) const { return !(*this == rhs); }

  /** The hash code for this value. */
  std::size_t hash_code() const;

  /**
   * A string useful for debugging.
   *
   * Format may change, and computation may be expensive.
   */
  std::string ToDebugString() const;

 private:
  ErrorData errors_;
};

inline std::ostream& operator<<(std::ostream& os, const Error& value) {
  return os << value.ToDebugString();
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ERROR_H_
