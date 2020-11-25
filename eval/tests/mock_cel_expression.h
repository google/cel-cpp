#ifndef THIRD_PARTY_CEL_CPP_EVAL_TESTS_MOCK_CEL_EXPRESION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_TESTS_MOCK_CEL_EXPRESION_H_

#include <memory>

#include "gmock/gmock.h"
#include "absl/status/statusor.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expression.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

class MockCelExpression : public CelExpression {
 public:
  MOCK_METHOD(std::unique_ptr<CelEvaluationState>, InitializeState,
              (google::protobuf::Arena * arena), (const, override));

  MOCK_METHOD(absl::StatusOr<CelValue>, Evaluate,
              (const BaseActivation& activation, google::protobuf::Arena* arena),
              (const, override));

  MOCK_METHOD(absl::StatusOr<CelValue>, Evaluate,
              (const BaseActivation& activation, CelEvaluationState* state),
              (const, override));

  MOCK_METHOD(absl::StatusOr<CelValue>, Trace,
              (const BaseActivation& activation, google::protobuf::Arena* arena,
               CelEvaluationListener callback),
              (const, override));

  MOCK_METHOD(absl::StatusOr<CelValue>, Trace,
              (const BaseActivation& activation, CelEvaluationState* state,
               CelEvaluationListener callback),
              (const, override));
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_TESTS_MOCK_CEL_EXPRESION_H_
