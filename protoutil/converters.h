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
common::Value ValueFrom(const google::protobuf::Value& value);
common::Value ValueFrom(google::protobuf::Value&& value);
common::Value ValueFrom(std::unique_ptr<google::protobuf::Value> value);
common::Value ValueFor(const google::protobuf::Value* value,
                       common::ParentRef parent = common::NoParent());

// Converters for google::protobuf::Struct.
common::Value ValueFrom(const google::protobuf::Struct& value);
common::Value ValueFrom(std::unique_ptr<google::protobuf::Struct> value);
common::Value ValueFor(const google::protobuf::Struct* value,
                       common::ParentRef parent = common::NoParent());

// Converters for google::protobuf::ListValue
common::Value ValueFrom(const google::protobuf::ListValue& value);
common::Value ValueFrom(std::unique_ptr<google::protobuf::ListValue> value);
common::Value ValueFor(const google::protobuf::ListValue* value,
                       common::ParentRef parent = common::NoParent());

// Converters for time/duration.
common::Value ValueFrom(const google::protobuf::Duration& value);
common::Value ValueFrom(const google::protobuf::Timestamp& value);

// Converters for wrapped values.
inline common::Value ValueFrom(google::protobuf::NullValue value) {
  return common::Value::NullValue();
}
inline common::Value ValueFrom(const google::protobuf::BoolValue& value) {
  return common::Value::FromBool(value.value());
}
inline common::Value ValueFrom(const google::protobuf::Int32Value& value) {
  return common::Value::FromInt(value.value());
}
inline common::Value ValueFrom(const google::protobuf::Int64Value& value) {
  return common::Value::FromInt(value.value());
}
inline common::Value ValueFrom(const google::protobuf::UInt32Value& value) {
  return common::Value::FromUInt(value.value());
}
inline common::Value ValueFrom(const google::protobuf::UInt64Value& value) {
  return common::Value::FromUInt(value.value());
}
inline common::Value ValueFrom(const google::protobuf::FloatValue& value) {
  return common::Value::FromDouble(value.value());
}
inline common::Value ValueFrom(const google::protobuf::DoubleValue& value) {
  return common::Value::FromDouble(value.value());
}
inline common::Value ValueFrom(const google::protobuf::StringValue& value) {
  return common::Value::FromString(value.value());
}
inline common::Value ValueFrom(const google::protobuf::BytesValue& value) {
  return common::Value::FromBytes(std::string(value.value()));
}

}  // namespace protoutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_PROTO_CONVERTERS_H_
