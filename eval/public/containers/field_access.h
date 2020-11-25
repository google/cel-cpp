#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_FIELD_ACCESS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_FIELD_ACCESS_H_

#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Creates CelValue from singular message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// arena Arena object to allocate result on, if needed.
// result pointer to CelValue to store the result in.
absl::Status CreateValueFromSingleField(const google::protobuf::Message* msg,
                                        const google::protobuf::FieldDescriptor* desc,
                                        google::protobuf::Arena* arena, CelValue* result);

// Creates CelValue from repeated message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// arena Arena object to allocate result on, if needed.
// index position in the repeated field.
// result pointer to CelValue to store the result in.
absl::Status CreateValueFromRepeatedField(const google::protobuf::Message* msg,
                                          const google::protobuf::FieldDescriptor* desc,
                                          google::protobuf::Arena* arena, int index,
                                          CelValue* result);

// Creates CelValue from map message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// value_ref pointer to map value.
// arena Arena object to allocate result on, if needed.
// result pointer to CelValue to store the result in.
absl::Status CreateValueFromMapValue(const google::protobuf::Message* msg,
                                     const google::protobuf::FieldDescriptor* desc,
                                     const google::protobuf::MapValueConstRef* value_ref,
                                     google::protobuf::Arena* arena, CelValue* result);

// Assigns content of CelValue to singular message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
absl::Status SetValueToSingleField(const CelValue& value,
                                   const google::protobuf::FieldDescriptor* desc,
                                   google::protobuf::Message* msg);
// Adds content of CelValue to repeated message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
absl::Status AddValueToRepeatedField(const CelValue& value,
                                     const google::protobuf::FieldDescriptor* desc,
                                     google::protobuf::Message* msg);

// Adds content of CelValue to repeated message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.

absl::Status AddValueToMapField(const CelValue& key, const CelValue& value,
                                const google::protobuf::FieldDescriptor* desc,
                                google::protobuf::Message* msg);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_FIELD_ACCESS_H_
