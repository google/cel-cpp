#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

bool CelFunction::MatchArguments(absl::Span<const CelValue> arguments) const {
  int types_size = descriptor().types.size();

  if (types_size != arguments.size()) {
    return false;
  }

  for (int i = 0; i < types_size; i++) {
    const auto& value = arguments[i];
    CelValue::Type arg_type = descriptor().types[i];
    if (value.type() != arg_type && arg_type != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

cel_base::Status CelFunctionRegistry::Register(
    std::unique_ptr<CelFunction> function) {
  const CelFunction::Descriptor& descriptor = function->descriptor();

  if (!FindOverloads(descriptor.name, descriptor.receiver_style,
                     descriptor.types)
           .empty()) {
    return cel_base::Status(
        cel_base::StatusCode::kAlreadyExists,
        "CelFunction with specified parameters already registered");
  }

  auto& overloads = functions_[descriptor.name];
  overloads.push_back(std::move(function));
  return cel_base::OkStatus();
}

std::vector<const CelFunction*> CelFunctionRegistry::FindOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types) const {
  std::vector<const CelFunction*> matched_funcs;

  int types_size = types.size();

  auto overloads = functions_.find(std::string(name));
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& func_ptr : overloads->second) {
    const CelFunction::Descriptor& other_desc = func_ptr->descriptor();
    if (other_desc.receiver_style != receiver_style) {
      continue;
    }

    if (other_desc.types.size() != types_size) {
      continue;
    }

    bool arg_match = true;
    for (int i = 0; i < types_size; i++) {
      CelValue::Type type0 = types[i];
      CelValue::Type type1 = other_desc.types[i];
      if (type0 != CelValue::Type::kAny && type1 != CelValue::Type::kAny &&
          type0 != type1) {
        arg_match = false;
        break;
      }
    }

    if (arg_match) {
      matched_funcs.push_back(func_ptr.get());
    }
  }

  return matched_funcs;
}

absl::node_hash_map<std::string, std::vector<const CelFunction::Descriptor*>>
CelFunctionRegistry::ListFunctions() const {
  absl::node_hash_map<std::string, std::vector<const CelFunction::Descriptor*>>
      descriptor_map;

  for (const auto& entry : functions_) {
    std::vector<const CelFunction::Descriptor*> descriptors;
    descriptors.reserve(entry.second.size());
    for (const auto& func : entry.second) {
      descriptors.push_back(&func->descriptor());
    }
    descriptor_map[entry.first] = std::move(descriptors);
  }

  return descriptor_map;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
