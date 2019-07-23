#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/flat_hash_map.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// A transformation over input expression that produces a new expression with
// constant sub-expressions replaced by generated idents in the constant_idents
// map. This transformation preserves the IDs of the input sub-expressions.
void FoldConstants(const google::api::expr::v1alpha1::Expr& expr,
                   const CelFunctionRegistry& registry, google::protobuf::Arena* arena,
                   absl::flat_hash_map<std::string, CelValue>& constant_idents,
                   google::api::expr::v1alpha1::Expr* out);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
