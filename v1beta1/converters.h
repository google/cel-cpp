#ifndef THIRD_PARTY_CEL_CPP_COMMON_V1_BETA1_CONVERTERS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_V1_BETA1_CONVERTERS_H_

#include "google/api/expr/v1beta1/eval.pb.h"
#include "google/api/expr/v1beta1/value.pb.h"
#include "common/converters.h"
#include "common/value.h"
#include "protoutil/type_registry.h"

namespace google {
namespace api {
namespace expr {
namespace v1beta1 {

/** Decode a v1beta1::Value. */
common::Value ValueFrom(const v1beta1::Value& value,
                        const protoutil::TypeRegistry* registry);
/** Decode a v1beta1::Value. */
common::Value ValueFrom(v1beta1::Value&& value,
                        const protoutil::TypeRegistry* registry);
/** Decode a v1beta1::Value. */
common::Value ValueFrom(std::unique_ptr<v1beta1::Value> value,
                        const protoutil::TypeRegistry* registry);
/** Decode a v1beta1::Value. */
common::Value ValueFor(const v1beta1::Value* value,
                       const protoutil::TypeRegistry* registry);

/** Decode a v1beta1::ExprValue. */
common::Value ValueFrom(const v1beta1::ExprValue& value,
                        const protoutil::TypeRegistry* registry);
/** Decode a v1beta1::ExprValue. */
common::Value ValueFrom(v1beta1::ExprValue&& value,
                        const protoutil::TypeRegistry* registry);
/** Decode a v1beta1::ExprValue. */
common::Value ValueFrom(std::unique_ptr<v1beta1::ExprValue> value,
                        const protoutil::TypeRegistry* registry);
/** Decode a v1beta1::ExprValue. */
common::Value ValueFor(const v1beta1::ExprValue* value,
                       const protoutil::TypeRegistry* registry);

/** Encode a v1beta1::ExprValue. */
google::rpc::Status ValueTo(const common::Value& value,
                            v1beta1::ExprValue* result);

}  // namespace v1beta1
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_V1_BETA1_CONVERTERS_H_
