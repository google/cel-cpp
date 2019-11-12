#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_H_

#include "absl/types/span.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelFunction is a handler that represents single
// CEL function.
// CelFunction provides Evaluate() method, that performs
// evaluation of the function. CelFunction instances provide
// descriptors that contain function information:
// - name
// - is function receiver style (e.f(g) vs f(e,g))
// - return type
// - amount of arguments and their types.
// Function overloads are resolved based on their arguments and
// receiver style.
class CelFunction {
 public:
  // Type that describes CelFunction.
  // This complex structure is needed for overloads support.
  //
  struct Descriptor {
    std::string name;
    bool receiver_style;
    std::vector<CelValue::Type> types;
  };

  // Build CelFunction from descriptor
  explicit CelFunction(const Descriptor& descriptor)
      : descriptor_(descriptor) {}

  // Non-copyable
  CelFunction(const CelFunction& other) = delete;
  CelFunction& operator=(const CelFunction& other) = delete;

  virtual ~CelFunction() {}

  // Evaluates CelValue based on arguments supplied.
  // If result content is to be allocated (e.g. string concatenation),
  // arena parameter must be used as allocation manager.
  // Provides resulting value in *result, returns evaluation success/failure.
  // Methods should discriminate between internal evaluator errors, that
  // makes further evaluation impossible or unreasonable (example - argument
  // type or number mismatch) and business logic errors (example division by
  // zero). When former happens, error Status is returned and *result is
  // not changed. In case of business logic error, returned Status is Ok, and
  // error is provided as CelValue - wrapped CelError in *result.
  virtual ::cel_base::Status Evaluate(absl::Span<const CelValue> arguments,
                                  CelValue* result,
                                  google::protobuf::Arena* arena) const = 0;

  // Determines whether instance of CelFunction is applicable to
  // arguments supplied.
  // Method is called during runtime.
  bool MatchArguments(absl::Span<const CelValue> arguments) const;

  // CelFunction descriptor
  const Descriptor& descriptor() const { return descriptor_; }

 private:
  Descriptor descriptor_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_H_
