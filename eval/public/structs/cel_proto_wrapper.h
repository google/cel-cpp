#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_WRAPPER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_WRAPPER_H_

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "eval/public/cel_value.h"
#include "internal/proto_util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
class CelProtoWrapper {
 public:
  // CreateMessage creates CelValue from google::protobuf::Message.
  // As some of CEL basic types are subclassing google::protobuf::Message,
  // this method contains type checking and downcasts.
  static CelValue CreateMessage(const google::protobuf::Message *value,
                                google::protobuf::Arena *arena);

  // CreateDuration creates CelValue from a non-null protobuf duration value.
  static CelValue CreateDuration(const google::protobuf::Duration *value) {
    return CelValue(expr::internal::DecodeDuration(*value));
  }

  // CreateTimestamp creates CelValue from a non-null protobuf timestamp value.
  static CelValue CreateTimestamp(const google::protobuf::Timestamp *value) {
    return CelValue(expr::internal::DecodeTime(*value));
  }
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_WRAPPER_H_
