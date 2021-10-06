#include "eval/public/cel_function_provider.h"

#include <memory>

#include "absl/status/statusor.h"
#include "eval/public/base_activation.h"

namespace google::api::expr::runtime {

namespace {
// Impl for simple provider that looks up functions in an activation function
// registry.
class ActivationFunctionProviderImpl : public CelFunctionProvider {
 public:
  ActivationFunctionProviderImpl() {}
  absl::StatusOr<const CelFunction*> GetFunction(
      const CelFunctionDescriptor& descriptor,
      const BaseActivation& activation) const override {
    std::vector<const CelFunction*> overloads =
        activation.FindFunctionOverloads(descriptor.name());

    const CelFunction* matching_overload = nullptr;

    for (const CelFunction* overload : overloads) {
      if (overload->descriptor().ShapeMatches(descriptor)) {
        if (matching_overload != nullptr) {
          return absl::Status(absl::StatusCode::kInvalidArgument,
                              "Couldn't resolve function.");
        }
        matching_overload = overload;
      }
    }

    return matching_overload;
  }
};

}  // namespace

std::unique_ptr<CelFunctionProvider> CreateActivationFunctionProvider() {
  return std::make_unique<ActivationFunctionProviderImpl>();
}

}  // namespace google::api::expr::runtime
