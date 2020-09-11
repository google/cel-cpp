#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_

#include "google/api/expr/v1alpha1/value.pb.h"
#include "absl/status/status.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::Status CelValueToValue(const CelValue& value, Value* result);

// TODO(issues/88) Add the notion of hashing and equivalence to CelValue and
// use that instead.
struct ValueInterner {
  size_t operator()(const Value& value) const;

  bool operator()(const Value& lhs, const Value& rhs) const;
};

}  // namespace runtime

}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_
