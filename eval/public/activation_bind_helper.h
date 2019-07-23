#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_BIND_HELPER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_BIND_HELPER_H_

#include "eval/public/activation.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Utility method, that takes a protobuf Message and interprets it as a
// namespace, binding its fields to Activation.
// Field names and values become respective names and values of parameters
// bound to the Activation object.
// Example:
// Assume we have a protobuf message of type:
// message Person {
//   int age = 1;
//   string name = 2;
// }
//
// The sample code snippet will look as follows:
//
//   Person person;
//   person.set_name("John Doe");
//   person.age(42);
//
//   RETURN_IF_ERROR(BindProtoToActivation(&person, &arena, &activation));
//
// After this snippet, activation will have two parameters bound:
//  "name", with string value of "John Doe"
//  "age", with int value of 42.
//
::cel_base::Status BindProtoToActivation(const google::protobuf::Message* message,
                                     google::protobuf::Arena* arena,
                                     Activation* activation);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_BIND_HELPER_H_
