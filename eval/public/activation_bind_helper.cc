#include "eval/public/activation_bind_helper.h"

#include "eval/eval/field_access.h"
#include "eval/eval/field_backed_list_impl.h"
#include "eval/eval/field_backed_map_impl.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::protobuf::Arena;
using google::protobuf::Message;
using google::protobuf::FieldDescriptor;
using google::protobuf::Descriptor;

cel_base::Status CreateValueFromField(const google::protobuf::Message* msg,
                                  const FieldDescriptor* field_desc,
                                  google::protobuf::Arena* arena, CelValue* result) {
  if (field_desc->is_map()) {
    *result = CelValue::CreateMap(google::protobuf::Arena::Create<FieldBackedMapImpl>(
        arena, msg, field_desc, arena));
    return cel_base::OkStatus();
  } else if (field_desc->is_repeated()) {
    *result = CelValue::CreateList(google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, msg, field_desc, arena));
    return cel_base::OkStatus();
  } else {
    return CreateValueFromSingleField(msg, field_desc, arena, result);
  }
}

}  // namespace

::cel_base::Status BindProtoToActivation(const Message* message, Arena* arena,
                                     Activation* activation) {
  // TODO(issues/24): Improve the utilities to bind dynamic values as well.
  const Descriptor* desc = message->GetDescriptor();
  const google::protobuf::Reflection* reflection = message->GetReflection();
  for (int i = 0; i < desc->field_count(); i++) {
    CelValue value;
    const FieldDescriptor* field_desc = desc->field(i);

    if (!field_desc->is_repeated() &&
        !reflection->HasField(*message, field_desc)) {
      continue;
    }

    auto status = CreateValueFromField(message, field_desc, arena, &value);
    if (!status.ok()) {
      return status;
    }

    activation->InsertValue(field_desc->name(), value);
  }

  return ::cel_base::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
