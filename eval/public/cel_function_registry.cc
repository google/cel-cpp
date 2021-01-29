#include "eval/public/cel_function_registry.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::Status CelFunctionRegistry::Register(
    std::unique_ptr<CelFunction> function) {
  const CelFunctionDescriptor& descriptor = function->descriptor();

  if (DescriptorRegistered(descriptor)) {
    return absl::Status(
        absl::StatusCode::kAlreadyExists,
        "CelFunction with specified parameters already registered");
  }

  auto& overloads = functions_[descriptor.name()];
  overloads.static_overloads.push_back(std::move(function));
  return absl::OkStatus();
}

absl::Status CelFunctionRegistry::RegisterLazyFunction(
    const CelFunctionDescriptor& descriptor,
    std::unique_ptr<CelFunctionProvider> factory) {
  if (DescriptorRegistered(descriptor)) {
    return absl::Status(
        absl::StatusCode::kAlreadyExists,
        "CelFunction with specified parameters already registered");
  }
  auto& overloads = functions_[descriptor.name()];
  LazyFunctionEntry entry = std::make_unique<LazyFunctionEntry::element_type>(
      descriptor, std::move(factory));
  overloads.lazy_overloads.push_back(std::move(entry));

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

  for (const auto& func_ptr : overloads->second.static_overloads) {
    if (func_ptr->descriptor().ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back(func_ptr.get());
    }
  }

  return matched_funcs;
}

std::vector<const CelFunctionProvider*> CelFunctionRegistry::FindLazyOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types) const {
  std::vector<const CelFunctionProvider*> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const LazyFunctionEntry& entry : overloads->second.lazy_overloads) {
    if (entry->first.ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back(entry->second.get());
    }
  }

  return matched_funcs;
}

absl::node_hash_map<std::string, std::vector<const CelFunctionDescriptor*>>
CelFunctionRegistry::ListFunctions() const {
  absl::node_hash_map<std::string, std::vector<const CelFunctionDescriptor*>>
      descriptor_map;

  for (const auto& entry : functions_) {
    std::vector<const CelFunctionDescriptor*> descriptors;
    const RegistryEntry& function_entry = entry.second;
    descriptors.reserve(function_entry.static_overloads.size() +
                        function_entry.lazy_overloads.size());
    for (const auto& func : function_entry.static_overloads) {
      descriptors.push_back(&func->descriptor());
    }
    for (const LazyFunctionEntry& func : function_entry.lazy_overloads) {
      descriptors.push_back(&func->first);
    }
    descriptor_map[entry.first] = std::move(descriptors);
  }

  return descriptor_map;
}

bool CelFunctionRegistry::DescriptorRegistered(
    const CelFunctionDescriptor& descriptor) const {
  return !(FindOverloads(descriptor.name(), descriptor.receiver_style(),
                         descriptor.types())
               .empty()) ||
         !(FindLazyOverloads(descriptor.name(), descriptor.receiver_style(),
                             descriptor.types())
               .empty());
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
