#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/status.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/memory/memory.h"
#include "absl/time/time.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

struct DefaultProtoEqual {
  inline bool operator()(const google::protobuf::Message& lhs,
                         const google::protobuf::Message& rhs) const {
    return google::protobuf::util::MessageDifferencer::Equals(lhs, rhs);
  }
};

/** Helper function to encode a duration in a google::protobuf::Duration. */
google::rpc::Status EncodeDuration(absl::Duration duration,
                                   google::protobuf::Duration* proto);

/** Helper function to encode a time in a google::protobuf::Timestamp. */
google::rpc::Status EncodeTime(absl::Time time,
                               google::protobuf::Timestamp* proto);

/** Helper function to decode a duration from a google::protobuf::Duration. */
absl::Duration DecodeDuration(const google::protobuf::Duration& proto);

/** Helper function to decode a time from a google::protobuf::Timestamp. */
absl::Time DecodeTime(const google::protobuf::Timestamp& proto);

/** Returns the min absl::Duration that can be represented as
/ * google::protobuf::Duration. */
inline absl::Duration MakeGoogleApiDurationMin() {
  return absl::Seconds(-315576000000) + absl::Nanoseconds(-999999999);
}

/** Returns the max absl::Duration that can be represented as
/ * google::protobuf::Duration. */
inline absl::Duration MakeGoogleApiDurationMax() {
  return absl::Seconds(315576000000) + absl::Nanoseconds(999999999);
}

/** Returns the min absl::Time that can be represented as
/ * google::protobuf::Timestamp. */
inline absl::Time MakeGoogleApiTimeMin() {
  return absl::UnixEpoch() + absl::Seconds(-62135596800);
}

/** Returns the max absl::Time that can be represented as
/ * google::protobuf::Timestamp. */
inline absl::Time MakeGoogleApiTimeMax() {
  return absl::UnixEpoch() + absl::Seconds(253402300799) +
         absl::Nanoseconds(999999999);
}

inline std::unique_ptr<google::protobuf::Message> Clone(const google::protobuf::Message& value) {
  auto result = absl::WrapUnique(value.New());
  result->CopyFrom(value);
  return result;
}

inline std::unique_ptr<google::protobuf::Message> Clone(google::protobuf::Message&& value) {
  auto result = absl::WrapUnique(value.New());
  result->GetReflection()->Swap(&value, result.get());
  return result;
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_
