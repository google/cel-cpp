#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_LIST_IMPL_H_

#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelList implementation that uses "repeated" message field
// as backing storage.
class FieldBackedListImpl : public CelList {
 public:
  // message contains the "repeated" field
  // descriptor FieldDescriptor for the field
  FieldBackedListImpl(const google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* descriptor,
                      google::protobuf::Arena* arena)
      : message_(message),
        descriptor_(descriptor),
        reflection_(message_->GetReflection()),
        arena_(arena) {}

  // List size.
  int size() const override;

  // List element access operator.
  CelValue operator[](int index) const override;

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::Reflection* reflection_;
  google::protobuf::Arena* arena_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_LIST_IMPL_H_
