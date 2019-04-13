#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_FIELD_BACKED_MAP_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_FIELD_BACKED_MAP_IMPL_H_

#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelMap implementation that uses "map" message field
// as backing storage.
class FieldBackedMapImpl : public CelMap {
 public:
  // message contains the "map" field. Object stores the pointer
  // to the message, thus it is expected that message outlives the
  // object.
  // descriptor FieldDescriptor for the field
  FieldBackedMapImpl(const google::protobuf::Message* message,
                     const google::protobuf::FieldDescriptor* descriptor,
                     google::protobuf::Arena* arena);

  // Map size.
  int size() const override;

  // Map element access operator.
  absl::optional<CelValue> operator[](CelValue key) const override;

  const CelList* ListKeys() const override;

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::Reflection* reflection_;
  google::protobuf::Arena* arena_;
  std::unique_ptr<CelList> key_list_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_FIELD_BACKED_MAP_IMPL_H_
