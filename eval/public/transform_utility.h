#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_

#include "cel/expr/value.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using cel::expr::Value;

// Translates a CelValue into a cel::expr::Value. Returns an error if
// translation is not supported.
absl::Status CelValueToValue(const CelValue& value, Value* result,
                             google::protobuf::Arena* arena);

inline absl::Status CelValueToValue(const CelValue& value, Value* result) {
  google::protobuf::Arena arena;
  return CelValueToValue(value, result, &arena);
}

// Translates a cel::expr::Value into a CelValue. Allocates any required
// external data on the provided arena. Returns an error if translation is not
// supported.
absl::StatusOr<CelValue> ValueToCelValue(const Value& value,
                                         google::protobuf::Arena* arena);

// BEGIN_INTERNAL

// TODO(issues/88) Add the notion of hashing and equivalence to CelValue and
// use that instead.
struct ValueInterner {
  size_t operator()(const Value& value) const;

  bool operator()(const Value& lhs, const Value& rhs) const;
};

// END_INTERNAL

}  // namespace runtime

}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_
