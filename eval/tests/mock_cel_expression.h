#ifndef THIRD_PARTY_CEL_CPP_EVAL_TESTS_MOCK_CEL_EXPRESION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_TESTS_MOCK_CEL_EXPRESION_H_

#include <memory>

#include "gmock/gmock.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expression.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

class MockCelExpression : public CelExpression {
 public:
  MOCK_CONST_METHOD1(InitializeState,
                     std::unique_ptr<CelEvaluationState>(google::protobuf::Arena* arena));

  MOCK_CONST_METHOD2(
      Evaluate, ::cel_base::StatusOr<CelValue>(const BaseActivation& activation,
                                           google::protobuf::Arena* arena));

  MOCK_CONST_METHOD2(
      Evaluate, ::cel_base::StatusOr<CelValue>(const BaseActivation& activation,
                                           CelEvaluationState* state));

  MOCK_CONST_METHOD3(
      Trace, ::cel_base::StatusOr<CelValue>(const BaseActivation& activation,
                                        google::protobuf::Arena* arena,
                                        CelEvaluationListener callback));

  MOCK_CONST_METHOD3(
      Trace, ::cel_base::StatusOr<CelValue>(const BaseActivation& activation,
                                        CelEvaluationState* state,
                                        CelEvaluationListener callback));
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_TESTS_MOCK_CEL_EXPRESION_H_
