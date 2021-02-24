#include "internal/proto_util.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/macros.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

namespace {

absl::Status Validate(absl::Duration duration) {
  if (duration < MakeGoogleApiDurationMin()) {
    return absl::InvalidArgumentError("duration below min");
  }

  if (duration > MakeGoogleApiDurationMax()) {
    return absl::InvalidArgumentError("duration above max");
  }
  return absl::OkStatus();
}

absl::Status Validate(absl::Time time) {
  if (time < MakeGoogleApiTimeMin()) {
    return absl::InvalidArgumentError("time below min");
  }

  if (time > MakeGoogleApiTimeMax()) {
    return absl::InvalidArgumentError("time above max");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Duration DecodeDuration(const google::protobuf::Duration& proto) {
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

absl::Time DecodeTime(const google::protobuf::Timestamp& proto) {
  return absl::FromUnixSeconds(proto.seconds()) +
         absl::Nanoseconds(proto.nanos());
}

absl::Status EncodeDuration(absl::Duration duration,
                            google::protobuf::Duration* proto) {
  RETURN_IF_ERROR(Validate(duration));
  // s and n may both be negative, per the Duration proto spec.
  const int64_t s = absl::IDivDuration(duration, absl::Seconds(1), &duration);
  const int64_t n = absl::IDivDuration(duration, absl::Nanoseconds(1), &duration);
  proto->set_seconds(s);
  proto->set_nanos(n);
  return absl::OkStatus();
}

absl::Status EncodeTime(absl::Time time, google::protobuf::Timestamp* proto) {
  RETURN_IF_ERROR(Validate(time));
  const int64_t s = absl::ToUnixSeconds(time);
  proto->set_seconds(s);
  proto->set_nanos((time - absl::FromUnixSeconds(s)) / absl::Nanoseconds(1));
  return absl::OkStatus();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
