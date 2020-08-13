#ifndef THIRD_PARTY_CEL_CPP_EVAL_STRUCTS_CEL_PROTO_WRAPPER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_STRUCTS_CEL_PROTO_WRAPPER_H_
#include "eval/public/cel_value.h"
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
                                google::protobuf::Arena *arena) {
    return CelValue::CreateMessage(value, arena);
  }
};
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
#endif  // THIRD_PARTY_CEL_CPP_EVAL_STRUCTS_CEL_PROTO_WRAPPER_H_
