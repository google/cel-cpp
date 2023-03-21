#include "eval/public/cel_function_registry.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/function.h"
#include "base/function_interface.h"
#include "base/kind.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/value_factory.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {
namespace {

// Legacy cel function that proxies to the modern cel::Function interface.
//
// This is used to wrap new-style cel::Functions for clients consuming
// legacy CelFunction-based APIs. The evaluate implementation on this class
// should not be called by the CEL evaluator, but a sensible result is returned
// for unit tests that haven't been migrated to the new APIs yet.
class ProxyToModernCelFunction : public CelFunction {
 public:
  ProxyToModernCelFunction(const cel::FunctionDescriptor& descriptor,
                           const std::unique_ptr<cel::Function>& implementation)
      : CelFunction(descriptor), implementation_(implementation.get()) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    // This is only safe for use during interop where the MemoryManager is
    // assumed to always be backed by a google::protobuf::Arena instance. After all
    // dependencies on legacy CelFunction are removed, we can remove this
    // implementation.
    cel::extensions::ProtoMemoryManager memory_manager(arena);
    cel::TypeFactory type_factory(memory_manager);
    cel::TypeManager type_manager(type_factory, cel::TypeProvider::Builtin());
    cel::ValueFactory value_factory(type_manager);
    cel::FunctionEvaluationContext context(value_factory);

    std::vector<cel::Handle<cel::Value>> modern_args =
        cel::interop_internal::LegacyValueToModernValueOrDie(arena, args);

    CEL_ASSIGN_OR_RETURN(auto modern_result,
                         implementation_->Invoke(context, modern_args));

    *result = cel::interop_internal::ModernValueToLegacyValueOrDie(
        arena, modern_result);

    return absl::OkStatus();
  }

 private:
  // owned by the registry
  const cel::Function* implementation_;
};

// Impl for simple provider that looks up functions in an activation function
// registry.
class ActivationFunctionProviderImpl : public CelFunctionProvider {
 public:
  ActivationFunctionProviderImpl() = default;
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

// Create a CelFunctionProvider that just looks up the functions inserted in the
// Activation. This is a convenience implementation for a simple, common
// use-case.
std::unique_ptr<CelFunctionProvider> CreateActivationFunctionProvider() {
  return std::make_unique<ActivationFunctionProviderImpl>();
}

}  // namespace

absl::Status CelFunctionRegistry::Register(
    std::unique_ptr<CelFunction> function) {
  const CelFunctionDescriptor& descriptor = function->descriptor();

  return Register(descriptor, std::move(function));
}

absl::Status CelFunctionRegistry::Register(
    const cel::FunctionDescriptor& descriptor,
    std::unique_ptr<cel::Function> implementation) {
  if (DescriptorRegistered(descriptor)) {
    return absl::Status(
        absl::StatusCode::kAlreadyExists,
        "CelFunction with specified parameters already registered");
  }
  if (!ValidateNonStrictOverload(descriptor)) {
    return absl::Status(absl::StatusCode::kAlreadyExists,
                        "Only one overload is allowed for non-strict function");
  }

  auto& overloads = functions_[descriptor.name()];
  overloads.static_overloads.push_back(
      StaticFunctionEntry(descriptor, std::move(implementation)));
  return absl::OkStatus();
}

absl::Status CelFunctionRegistry::RegisterAll(
    std::initializer_list<Registrar> registrars,
    const InterpreterOptions& opts) {
  for (Registrar registrar : registrars) {
    CEL_RETURN_IF_ERROR(registrar(this, opts));
  }
  return absl::OkStatus();
}

absl::Status CelFunctionRegistry::RegisterLazyFunction(
    const CelFunctionDescriptor& descriptor) {
  if (DescriptorRegistered(descriptor)) {
    return absl::Status(
        absl::StatusCode::kAlreadyExists,
        "CelFunction with specified parameters already registered");
  }
  if (!ValidateNonStrictOverload(descriptor)) {
    return absl::Status(absl::StatusCode::kAlreadyExists,
                        "Only one overload is allowed for non-strict function");
  }
  auto& overloads = functions_[descriptor.name()];

  overloads.lazy_overloads.push_back(
      LazyFunctionEntry(descriptor, CreateActivationFunctionProvider()));

  return absl::OkStatus();
}

std::vector<const CelFunction*> CelFunctionRegistry::FindOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types) const {
  std::vector<const CelFunction*> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& overload : overloads->second.static_overloads) {
    if (overload.descriptor->ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back(overload.legacy_implementation.get());
    }
  }

  return matched_funcs;
}

std::vector<CelFunctionRegistry::StaticOverload>
CelFunctionRegistry::FindStaticOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types) const {
  std::vector<CelFunctionRegistry::StaticOverload> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& overload : overloads->second.static_overloads) {
    if (overload.descriptor->ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back(
          {overload.descriptor.get(), overload.implementation.get()});
    }
  }

  return matched_funcs;
}

std::vector<const CelFunctionDescriptor*>
CelFunctionRegistry::FindLazyOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types) const {
  std::vector<const CelFunctionDescriptor*> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& entry : overloads->second.lazy_overloads) {
    if (entry.descriptor->ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back(entry.descriptor.get());
    }
  }

  return matched_funcs;
}

std::vector<CelFunctionRegistry::LazyOverload>
CelFunctionRegistry::ModernFindLazyOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<cel::Kind>& types) const {
  std::vector<CelFunctionRegistry::LazyOverload> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& entry : overloads->second.lazy_overloads) {
    if (entry.descriptor->ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back(
          {entry.descriptor.get(), entry.function_provider.get()});
    }
  }

  return matched_funcs;
}

absl::node_hash_map<std::string, std::vector<const CelFunctionDescriptor*>>
CelFunctionRegistry::ListFunctions() const {
  absl::node_hash_map<std::string, std::vector<const CelFunctionDescriptor*>>
      descriptor_map;

  for (const auto& entry : functions_) {
    std::vector<const cel::FunctionDescriptor*> descriptors;
    const RegistryEntry& function_entry = entry.second;
    descriptors.reserve(function_entry.static_overloads.size() +
                        function_entry.lazy_overloads.size());
    for (const auto& entry : function_entry.static_overloads) {
      descriptors.push_back(entry.descriptor.get());
    }
    for (const auto& entry : function_entry.lazy_overloads) {
      descriptors.push_back(entry.descriptor.get());
    }
    descriptor_map[entry.first] = std::move(descriptors);
  }

  return descriptor_map;
}

bool CelFunctionRegistry::DescriptorRegistered(
    const cel::FunctionDescriptor& descriptor) const {
  return !(FindOverloads(descriptor.name(), descriptor.receiver_style(),
                         descriptor.types())
               .empty()) ||
         !(FindLazyOverloads(descriptor.name(), descriptor.receiver_style(),
                             descriptor.types())
               .empty());
}

bool CelFunctionRegistry::ValidateNonStrictOverload(
    const CelFunctionDescriptor& descriptor) const {
  auto overloads = functions_.find(descriptor.name());
  if (overloads == functions_.end()) {
    return true;
  }
  const RegistryEntry& entry = overloads->second;
  if (!descriptor.is_strict()) {
    // If the newly added overload is a non-strict function, we require that
    // there are no other overloads, which is not possible here.
    return false;
  }
  // If the newly added overload is a strict function, we need to make sure
  // that no previous overloads are registered non-strict. If the list of
  // overload is not empty, we only need to check the first overload. This is
  // because if the first overload is strict, other overloads must also be
  // strict by the rule.
  return (entry.static_overloads.empty() ||
          entry.static_overloads[0].descriptor->is_strict()) &&
         (entry.lazy_overloads.empty() ||
          entry.lazy_overloads[0].descriptor->is_strict());
}

CelFunctionRegistry::StaticFunctionEntry::StaticFunctionEntry(
    const cel::FunctionDescriptor& descriptor,
    std::unique_ptr<cel::Function> impl)
    : descriptor(std::make_unique<cel::FunctionDescriptor>(descriptor)),
      implementation(std::move(impl)),
      legacy_implementation(std::make_unique<ProxyToModernCelFunction>(
          descriptor, implementation)) {}

}  // namespace google::api::expr::runtime
