#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/node_hash_map.h"
#include "base/function.h"
#include "base/kind.h"
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
  // Represents a single overload for a function.
  struct StaticOverload {
    const cel::FunctionDescriptor* descriptor;
    const cel::Function* implementation;
  };

  // Represents a single overload for a lazily provided function.
  struct LazyOverload {
    const cel::FunctionDescriptor* descriptor;
    const CelFunctionProvider* provider;
  };

  CelFunctionRegistry() = default;

  ~CelFunctionRegistry() = default;

  using Registrar = absl::Status (*)(CelFunctionRegistry*,
                                     const InterpreterOptions&);

  // Register CelFunction object. Object ownership is
  // passed to registry.
  // Function registration should be performed prior to
  // CelExpression creation.
  absl::Status Register(std::unique_ptr<CelFunction> function);

  absl::Status Register(const cel::FunctionDescriptor& descriptor,
                        std::unique_ptr<cel::Function> implementation);

  absl::Status RegisterAll(std::initializer_list<Registrar> registrars,
                           const InterpreterOptions& opts);

  // Register a lazily provided function. CelFunctionProvider is used to get
  // a CelFunction ptr at evaluation time. The registry takes ownership of the
  // factory.
  ABSL_DEPRECATED(
      "Deprecated, prefer using the default implementation provided"
      " by the Descriptor only overload.")
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
  //
  // Results refer to underlying registry entries by pointer. Results are
  // invalid after the registry is deleted.
  std::vector<const CelFunction*> FindOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types) const;

  std::vector<StaticOverload> FindStaticOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<cel::Kind>& types) const;

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

  // TODO(issues/5): Update to after introducing new equivalent for Lazily
  // bound functions.
  std::vector<LazyOverload> ModernFindLazyOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types) const;

  // Retrieve list of registered function descriptors. This includes both
  // static and lazy functions.
  absl::node_hash_map<std::string, std::vector<const cel::FunctionDescriptor*>>
  ListFunctions() const;

 private:
  struct StaticFunctionEntry {
    StaticFunctionEntry(const cel::FunctionDescriptor& descriptor,
                        std::unique_ptr<cel::Function> impl);
    // Extra indirection needed to preserve pointer stability for the
    // descriptors.
    std::unique_ptr<cel::FunctionDescriptor> descriptor;
    std::unique_ptr<cel::Function> implementation;
    std::unique_ptr<CelFunction> legacy_implementation;
  };

  struct LazyFunctionEntry {
    LazyFunctionEntry(const cel::FunctionDescriptor& descriptor,
                      std::unique_ptr<CelFunctionProvider> provider)
        : descriptor(std::make_unique<cel::FunctionDescriptor>(descriptor)),
          function_provider(std::move(provider)) {}

    // Extra indirection needed to preserve pointer stability for the
    // descriptors.
    std::unique_ptr<cel::FunctionDescriptor> descriptor;
    std::unique_ptr<CelFunctionProvider> function_provider;
  };

  struct RegistryEntry {
    std::vector<StaticFunctionEntry> static_overloads;
    std::vector<LazyFunctionEntry> lazy_overloads;
  };
  // Returns whether the descriptor is registered in either as a lazy funtion or
  // in the static functions.
  bool DescriptorRegistered(const CelFunctionDescriptor& descriptor) const;
  // Returns true if after adding this function, the rule "a non-strict
  // function should have only a single overload" will be preserved.
  bool ValidateNonStrictOverload(const CelFunctionDescriptor& descriptor) const;

  // indexed by function name (not type checker overload id).
  absl::flat_hash_map<std::string, RegistryEntry> functions_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_
