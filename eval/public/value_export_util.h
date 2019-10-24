#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_VALUE_EXPORT_UTIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_VALUE_EXPORT_UTIL_H_

#include "eval/public/cel_value.h"

#include "google/protobuf/struct.pb.h"
#include "base/status.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Exports content of CelValue as google.protobuf.Value.
// Current limitations:
// - exports integer values as doubles (Value.number_value);
// - exports integer keys in maps as strings;
// - handles Duration and Timestamp as generic messages.
cel_base::Status ExportAsProtoValue(const CelValue &in_value,
                                google::protobuf::Value *out_value);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_VALUE_EXPORT_UTIL_H_
