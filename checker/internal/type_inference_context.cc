// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/type_inference_context.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_kind.h"

namespace cel::checker_internal {
namespace {

bool IsWildCardType(Type type) {
  switch (type.kind()) {
    case TypeKind::kDyn:
    case TypeKind::kError:
      return true;
    default:
      return false;
  }
}

struct FunctionOverloadInstance {
  Type result_type;
  std::vector<Type> param_types;
};

FunctionOverloadInstance InstantiateFunctionOverload(
    TypeInferenceContext& inference_context, const OverloadDecl& ovl) {
  FunctionOverloadInstance result;
  result.param_types.reserve(ovl.args().size());

  TypeInferenceContext::InstanceMap substitutions;
  result.result_type =
      inference_context.InstantiateTypeParams(ovl.result(), substitutions);

  for (int i = 0; i < ovl.args().size(); ++i) {
    result.param_types.push_back(
        inference_context.InstantiateTypeParams(ovl.args()[i], substitutions));
  }
  return result;
}

}  // namespace

Type TypeInferenceContext::InstantiateTypeParams(const Type& type) {
  InstanceMap substitutions;
  return InstantiateTypeParams(type, substitutions);
}

Type TypeInferenceContext::InstantiateTypeParams(
    const Type& type,
    absl::flat_hash_map<std::string, absl::string_view>& substitutions) {
  switch (type.kind()) {
    // Unparameterized types -- just forward.
    case TypeKind::kBool:
    case TypeKind::kDouble:
    case TypeKind::kString:
    case TypeKind::kBytes:
    case TypeKind::kInt:
    case TypeKind::kUint:
    case TypeKind::kTimestamp:
    case TypeKind::kDuration:
    case TypeKind::kStruct:
    case TypeKind::kNull:
    case TypeKind::kError:
    case TypeKind::kBoolWrapper:
    case TypeKind::kDoubleWrapper:
    case TypeKind::kIntWrapper:
    case TypeKind::kUintWrapper:
    case TypeKind::kBytesWrapper:
    case TypeKind::kStringWrapper:
    case TypeKind::kDyn:
      return type;
    case TypeKind::kAny:
      return DynType();
    case TypeKind::kTypeParam: {
      absl::string_view name = type.AsTypeParam()->name();
      if (auto it = substitutions.find(name); it != substitutions.end()) {
        return TypeParamType(it->second);
      }
      absl::string_view substitution = NewTypeVar();
      substitutions[type.AsTypeParam()->name()] = substitution;
      return TypeParamType(substitution);
    }
    case TypeKind::kType: {
      auto type_type = type.AsType();
      auto parameters = type_type->GetParameters();
      if (parameters.size() == 1) {
        Type param = InstantiateTypeParams(parameters[0], substitutions);
        return TypeType(arena_, param);
      } else if (parameters.size() > 1) {
        return ErrorType();
      } else {  // generic type
        return type;
      }
    }
    case TypeKind::kList: {
      Type elem =
          InstantiateTypeParams(type.AsList()->element(), substitutions);
      return ListType(arena_, elem);
    }
    case TypeKind::kMap: {
      Type key = InstantiateTypeParams(type.AsMap()->key(), substitutions);
      Type value = InstantiateTypeParams(type.AsMap()->value(), substitutions);
      return MapType(arena_, key, value);
    }
    case TypeKind::kOpaque: {
      auto opaque_type = type.AsOpaque();
      auto parameters = opaque_type->GetParameters();
      std::vector<Type> param_instances;
      param_instances.reserve(parameters.size());

      for (int i = 0; i < parameters.size(); ++i) {
        param_instances.push_back(
            InstantiateTypeParams(parameters[i], substitutions));
      }
      return OpaqueType(arena_, type.AsOpaque()->name(), param_instances);
    }
    default:
      return ErrorType();
  }
}

bool TypeInferenceContext::IsAssignable(const Type& parameter,
                                        const Type& instance) {
  // Simple assignablility check assuming parameters are correctly bound.
  // TODO: handle resolving type parameter substitution.
  if (IsWildCardType(parameter) || IsWildCardType(instance)) {
    return true;
  }
  return common_internal::TypeIsAssignable(parameter, instance);
}

absl::optional<TypeInferenceContext::OverloadResolution>
TypeInferenceContext::ResolveOverload(const FunctionDecl& decl,
                                      absl::Span<const Type> argument_types,
                                      bool is_receiver) {
  absl::optional<Type> result_type;

  std::vector<OverloadDecl> matching_overloads;
  for (const auto& ovl : decl.overloads()) {
    if (ovl.member() != is_receiver ||
        argument_types.size() != ovl.args().size()) {
      continue;
    }

    auto call_type_instance = InstantiateFunctionOverload(*this, ovl);
    ABSL_DCHECK_EQ(argument_types.size(),
                   call_type_instance.param_types.size());
    bool is_match = true;
    for (int i = 0; i < argument_types.size(); ++i) {
      if (!IsAssignable(call_type_instance.param_types[i], argument_types[i])) {
        is_match = false;
        break;
      }
    }

    if (is_match) {
      matching_overloads.push_back(ovl);
      if (!result_type.has_value()) {
        result_type = call_type_instance.result_type;
      } else {
        if (!TypeEquivalent(*result_type, call_type_instance.result_type)) {
          result_type = DynType();
        }
      }
    }
  }

  if (!result_type.has_value() || matching_overloads.empty()) {
    return absl::nullopt;
  }
  return OverloadResolution{
      .result_type = *result_type,
      .overloads = std::move(matching_overloads),
  };
}

bool TypeInferenceContext::TypeEquivalent(Type a, Type b) { return a == b; }

}  // namespace cel::checker_internal
