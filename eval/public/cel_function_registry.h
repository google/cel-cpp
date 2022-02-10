#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_

#include "absl/container/node_hash_map.h"
#include "absl/types/span.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_provider.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// CelFunctionRegistry class allows to register builtin or custom
// CelFunction handlers with it and look them up when creating
// CelExpression objects from Expr ASTs.
class CelFunctionRegistry {
 public:
  CelFunctionRegistry() {}

  ~CelFunctionRegistry() {}

  // Register CelFunction object. Object ownership is
  // passed to registry.
  // Function registration should be performed prior to
  // CelExpression creation.
  absl::Status Register(std::unique_ptr<CelFunction> function);

  // Register a lazily provided function. CelFunctionProvider is used to get
  // a CelFunction ptr at evaluation time. The registry takes ownership of the
  // factory.
  absl::Status RegisterLazyFunction(
      const CelFunctionDescriptor& descriptor,
      std::unique_ptr<CelFunctionProvider> factory);

  // Register a lazily provided function. This overload uses a default provider
  // that delegates to the activation at evaluation time.
  absl::Status RegisterLazyFunction(const CelFunctionDescriptor& descriptor) {
    return RegisterLazyFunction(descriptor, CreateActivationFunctionProvider());
  }

  // Find subset of CelFunction that match overload conditions
  // As types may not be available during expression compilation,
  // further narrowing of this subset will happen at evaluation stage.
  // name - the name of CelFunction;
  // receiver_style - indicates whether function has receiver style;
  // types - argument types. If  type is not known during compilation,
  // DYN value should be passed.
  std::vector<const CelFunction*> FindOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types) const;

  // Find subset of CelFunction providers that match overload conditions
  // As types may not be available during expression compilation,
  // further narrowing of this subset will happen at evaluation stage.
  // name - the name of CelFunction;
  // receiver_style - indicates whether function has receiver style;
  // types - argument types. If  type is not known during compilation,
  // DYN value should be passed.
  std::vector<const CelFunctionProvider*> FindLazyOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types) const;

  // Retrieve list of registered function descriptors. This includes both
  // static and lazy functions.
  absl::node_hash_map<std::string, std::vector<const CelFunctionDescriptor*>>
  ListFunctions() const;

 private:
  // Returns whether the descriptor is registered in either as a lazy funtion or
  // in the static functions.
  bool DescriptorRegistered(const CelFunctionDescriptor& descriptor) const;
  // Returns true if after adding this function, the rule "a non-strict
  // function should have only a single overload" will be preserved.
  bool ValidateNonStrictOverload(const CelFunctionDescriptor& descriptor) const;

  using StaticFunctionEntry = std::unique_ptr<CelFunction>;
  using LazyFunctionEntry = std::unique_ptr<
      std::pair<CelFunctionDescriptor, std::unique_ptr<CelFunctionProvider>>>;
  struct RegistryEntry {
    std::vector<StaticFunctionEntry> static_overloads;
    std::vector<LazyFunctionEntry> lazy_overloads;
  };
  absl::node_hash_map<std::string, RegistryEntry> functions_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_
