#include "internal/proto_util.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/strings/str_cat.h"
#include "common/macros.h"
#include "internal/status_util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

namespace {

google::rpc::Status Validate(absl::Duration duration) {
  if (duration < MakeGoogleApiDurationMin()) {
    return InvalidArgumentError(absl::StrCat("duration below min"));
  }

  if (duration > MakeGoogleApiDurationMax()) {
    return InvalidArgumentError(absl::StrCat("duration above max"));
  }
  return OkStatus();
}

google::rpc::Status Validate(absl::Time time) {
  if (time < MakeGoogleApiTimeMin()) {
    return InvalidArgumentError(absl::StrCat("time below min"));
  }

  if (time > MakeGoogleApiTimeMax()) {
    return InvalidArgumentError(absl::StrCat("time above max"));
  }
  return OkStatus();
}

}  // namespace

absl::Duration DecodeDuration(const google::protobuf::Duration& proto) {
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

absl::Time DecodeTime(const google::protobuf::Timestamp& proto) {
  return absl::FromUnixSeconds(proto.seconds()) +
         absl::Nanoseconds(proto.nanos());
}

google::rpc::Status EncodeDuration(absl::Duration duration,
                                   google::protobuf::Duration* proto) {
  RETURN_IF_STATUS_ERROR(Validate(duration));
  // s and n may both be negative, per the Duration proto spec.
  const int64_t s = absl::IDivDuration(duration, absl::Seconds(1), &duration);
  const int64_t n = absl::IDivDuration(duration, absl::Nanoseconds(1), &duration);
  proto->set_seconds(s);
  proto->set_nanos(n);
  return OkStatus();
}

google::rpc::Status EncodeTime(absl::Time time,
                               google::protobuf::Timestamp* proto) {
  RETURN_IF_STATUS_ERROR(Validate(time));
  const int64_t s = absl::ToUnixSeconds(time);
  proto->set_seconds(s);
  proto->set_nanos((time - absl::FromUnixSeconds(s)) / absl::Nanoseconds(1));
  return OkStatus();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
