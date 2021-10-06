#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_PROVIDER_H_

#include <memory>

#include "absl/status/statusor.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_function.h"

namespace google::api::expr::runtime {

// CelFunctionProvider is an interface for providers of lazy CelFunctions (i.e.
// implementation isn't available until evaluation time based on the
// activation).
class CelFunctionProvider {
 public:
  // Returns a ptr to a |CelFunction| based on the provided |Activation|. Given
  // the same activation, this should return the same CelFunction. The
  // CelFunction ptr is assumed to be stable for the life of the Activation.
  // nullptr is interpreted as no funtion overload matches the descriptor.
  virtual absl::StatusOr<const CelFunction*> GetFunction(
      const CelFunctionDescriptor& descriptor,
      const BaseActivation& activation) const = 0;
  virtual ~CelFunctionProvider() {}
};

// Create a CelFunctionProvider that just looks up the functions inserted in the
// Activation. This is a convenience implementation for a simple, common
// use-case.
std::unique_ptr<CelFunctionProvider> CreateActivationFunctionProvider();

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_PROVIDER_H_
