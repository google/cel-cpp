
#include "eval/eval/field_backed_list_impl.h"
#include "eval/eval/field_access.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

int FieldBackedListImpl::size() const {
  return reflection_->FieldSize(*message_, descriptor_);
}

CelValue FieldBackedListImpl::operator[](int index) const {
  CelValue result = CelValue::CreateNull();
  auto status = CreateValueFromRepeatedField(message_, descriptor_, arena_,
                                             index, &result);
  if (!util::IsOk(status)) {
    CelError* error = google::protobuf::Arena::Create<CelError>(arena_);
    error->set_message(status.message());
    error->set_code(CelError::Code::CelError_Code_UNKNOWN);
    result = CelValue::CreateError(error);
  }

  return result;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
