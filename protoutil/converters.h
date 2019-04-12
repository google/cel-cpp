// Converter functions from common c++ representations to Value.

#ifndef THIRD_PARTY_CEL_CPP_PROTOUTIL_CONVERTERS_H_
#define THIRD_PARTY_CEL_CPP_PROTOUTIL_CONVERTERS_H_

#include <memory>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/message.h"
#include "common/parent_ref.h"
#include "common/value.h"
#include "protoutil/type_registry.h"

namespace google {
namespace api {
namespace expr {
namespace protoutil {

/** Registers all converter functions with the given type registry. */
bool RegisterConvertersWith(TypeRegistry* registry);

// Converters for google::protobuf::Value.
Value ValueFrom(const google::protobuf::Value& value);
Value ValueFrom(google::protobuf::Value&& value);
Value ValueFrom(std::unique_ptr<google::protobuf::Value> value);
Value ValueFor(const google::protobuf::Value* value,
               ParentRef parent = NoParent());

// Converters for google::protobuf::Struct.
Value ValueFrom(const google::protobuf::Struct& value);
Value ValueFrom(std::unique_ptr<google::protobuf::Struct> value);
Value ValueFor(const google::protobuf::Struct* value,
               ParentRef parent = NoParent());

// Converters for google::protobuf::ListValue
Value ValueFrom(const google::protobuf::ListValue& value);
Value ValueFrom(std::unique_ptr<google::protobuf::ListValue> value);
Value ValueFor(const google::protobuf::ListValue* value,
               ParentRef parent = NoParent());

// Converters for time/duration.
Value ValueFrom(const google::protobuf::Duration& value);
Value ValueFrom(const google::protobuf::Timestamp& value);

// Converters for wrapped values.
inline Value ValueFrom(google::protobuf::NullValue value) {
  return Value::NullValue();
}
inline Value ValueFrom(const google::protobuf::BoolValue& value) {
  return Value::FromBool(value.value());
}
inline Value ValueFrom(const google::protobuf::Int32Value& value) {
  return Value::FromInt(value.value());
}
inline Value ValueFrom(const google::protobuf::Int64Value& value) {
  return Value::FromInt(value.value());
}
inline Value ValueFrom(const google::protobuf::UInt32Value& value) {
  return Value::FromUInt(value.value());
}
inline Value ValueFrom(const google::protobuf::UInt64Value& value) {
  return Value::FromUInt(value.value());
}
inline Value ValueFrom(const google::protobuf::FloatValue& value) {
  return Value::FromDouble(value.value());
}
inline Value ValueFrom(const google::protobuf::DoubleValue& value) {
  return Value::FromDouble(value.value());
}
inline Value ValueFrom(const google::protobuf::StringValue& value) {
  return Value::FromString(value.value());
}
inline Value ValueFrom(const google::protobuf::BytesValue& value) {
  return Value::FromBytes(std::string(value.value()));
}

}  // namespace protoutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_PROTO_CONVERTERS_H_
