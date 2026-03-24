// Copyright 2026 Google LLC
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

#include "env/env.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/type_checker_builder.h"
#include "common/constant.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "env/config.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

bool ShouldIncludeMacro(const Config::StandardLibraryConfig& config,
                        absl::string_view macro) {
  if (config.disable_macros) {
    return false;
  }
  if (config.excluded_macros.contains(macro)) {
    return false;
  }
  if (!config.included_macros.empty() &&
      !config.included_macros.contains(macro)) {
    return false;
  }
  return true;
}

bool ShouldIncludeFunction(const Config::StandardLibraryConfig& config,
                           absl::string_view function,
                           absl::string_view overload_id) {
  if (config.excluded_functions.contains(
          std::make_pair(std::string(function), std::string(overload_id))) ||
      config.excluded_functions.contains(
          std::make_pair(std::string(function), ""))) {
    return false;
  }
  if (!config.included_functions.empty() &&
      !config.included_functions.contains(
          std::make_pair(std::string(function), "")) &&
      !config.included_functions.contains(
          std::make_pair(std::string(function), std::string(overload_id)))) {
    return false;
  }
  return true;
}

absl::StatusOr<CompilerLibrarySubset> MakeStdlibSubset(
    const Config::StandardLibraryConfig& standard_library_config) {
  CompilerLibrarySubset subset;
  subset.library_id = "stdlib";
  // Capturing by reference is safe. The returned CompilerLibrarySubset's
  // callbacks are only used during CompilerBuilder::Build() to configure
  // contributed functions and macros. They are not retained by the constructed
  // Compiler instance. The referenced config outlives the Build() call.
  subset.should_include_macro = [&standard_library_config](const Macro& macro) {
    return ShouldIncludeMacro(standard_library_config, macro.function());
  };
  subset.should_include_overload = [&standard_library_config](
                                       absl::string_view function,
                                       absl::string_view overload_id) {
    return ShouldIncludeFunction(standard_library_config, function,
                                 overload_id);
  };
  return subset;
}

std::optional<TypeKind> TypeNameToTypeKind(absl::string_view type_name) {
  // Excluded types:
  //   kUnknown
  //   kError
  //   kTypeParam
  //   kFunction
  //   kEnum

  static const absl::NoDestructor<
      absl::flat_hash_map<absl::string_view, TypeKind>>
      kTypeNameToTypeKind({
          {"null", TypeKind::kNull},
          {"bool", TypeKind::kBool},
          {"int", TypeKind::kInt},
          {"uint", TypeKind::kUint},
          {"double", TypeKind::kDouble},
          {"string", TypeKind::kString},
          {"bytes", TypeKind::kBytes},
          {"timestamp", TypeKind::kTimestamp},
          {TimestampType::kName, TypeKind::kTimestamp},
          {"duration", TypeKind::kDuration},
          {DurationType::kName, TypeKind::kDuration},
          {"list", TypeKind::kList},
          {"map", TypeKind::kMap},
          {"", TypeKind::kDyn},
          {"any", TypeKind::kAny},
          {"dyn", TypeKind::kDyn},
          {BoolWrapperType::kName, TypeKind::kBoolWrapper},
          {IntWrapperType::kName, TypeKind::kIntWrapper},
          {UintWrapperType::kName, TypeKind::kUintWrapper},
          {DoubleWrapperType::kName, TypeKind::kDoubleWrapper},
          {StringWrapperType::kName, TypeKind::kStringWrapper},
          {BytesWrapperType::kName, TypeKind::kBytesWrapper},
          {"type", TypeKind::kType},
      });
  if (auto it = kTypeNameToTypeKind->find(type_name);
      it != kTypeNameToTypeKind->end()) {
    return it->second;
  }

  return std::nullopt;
}

absl::StatusOr<Type> TypeInfoToType(
    const Config::TypeInfo& type_info, google::protobuf::Arena* arena,
    const google::protobuf::DescriptorPool* descriptor_pool) {
  if (type_info.is_type_param) {
    return TypeParamType(type_info.name);
  }

  std::optional<TypeKind> type_kind = TypeNameToTypeKind(type_info.name);
  if (!type_kind.has_value()) {
    if (type_info.params.empty() && descriptor_pool != nullptr) {
      const google::protobuf::Descriptor* type =
          descriptor_pool->FindMessageTypeByName(type_info.name);
      if (type != nullptr) {
        return MessageType(type);
      }
    }
    // TODO(uncreated-issue/88): use a TypeIntrospector to validate opaque types
    std::vector<Type> parameter_types;
    for (const Config::TypeInfo& param : type_info.params) {
      CEL_ASSIGN_OR_RETURN(Type parameter_type,
                           TypeInfoToType(param, arena, descriptor_pool));
      parameter_types.push_back(parameter_type);
    }

    return OpaqueType(arena, type_info.name, parameter_types);
  }

  switch (*type_kind) {
    case TypeKind::kNull:
      return NullType();
    case TypeKind::kBool:
      return BoolType();
    case TypeKind::kInt:
      return IntType();
    case TypeKind::kUint:
      return UintType();
    case TypeKind::kDouble:
      return DoubleType();
    case TypeKind::kString:
      return StringType();
    case TypeKind::kBytes:
      return BytesType();
    case TypeKind::kDuration:
      return DurationType();
    case TypeKind::kTimestamp:
      return TimestampType();
    case TypeKind::kList: {
      Type element_type;
      if (!type_info.params.empty()) {
        CEL_ASSIGN_OR_RETURN(
            element_type,
            TypeInfoToType(type_info.params[0], arena, descriptor_pool));
      } else {
        element_type = DynType();
      }
      return ListType(arena, element_type);
    }
    case TypeKind::kMap: {
      Type key_type = DynType();
      Type value_type = DynType();
      if (!type_info.params.empty()) {
        CEL_ASSIGN_OR_RETURN(key_type, TypeInfoToType(type_info.params[0],
                                                      arena, descriptor_pool));
      }
      if (type_info.params.size() > 1) {
        CEL_ASSIGN_OR_RETURN(
            value_type,
            TypeInfoToType(type_info.params[1], arena, descriptor_pool));
      }
      return MapType(arena, key_type, value_type);
    }
    case TypeKind::kDyn:
      return DynType();
    case TypeKind::kAny:
      return AnyType();
    case TypeKind::kBoolWrapper:
      return BoolWrapperType();
    case TypeKind::kIntWrapper:
      return IntWrapperType();
    case TypeKind::kUintWrapper:
      return UintWrapperType();
    case TypeKind::kDoubleWrapper:
      return DoubleWrapperType();
    case TypeKind::kStringWrapper:
      return StringWrapperType();
    case TypeKind::kBytesWrapper:
      return BytesWrapperType();
    case TypeKind::kType: {
      if (type_info.params.empty()) {
        return TypeType(arena, DynType());
      }
      CEL_ASSIGN_OR_RETURN(Type type, TypeInfoToType(type_info.params[0], arena,
                                                     descriptor_pool));
      return TypeType(arena, type);
    }
    default:
      return DynType();
  }
}

absl::StatusOr<FunctionDecl> FunctionConfigToFunctionDecl(
    const Config::FunctionConfig& function_config, google::protobuf::Arena* arena,
    const google::protobuf::DescriptorPool* descriptor_pool) {
  FunctionDecl function_decl;
  function_decl.set_name(function_config.name);
  for (const Config::FunctionOverloadConfig& overload_config :
       function_config.overload_configs) {
    OverloadDecl overload_decl;
    overload_decl.set_id(overload_config.overload_id);
    overload_decl.set_member(overload_config.is_member_function);
    for (const Config::TypeInfo& parameter : overload_config.parameters) {
      CEL_ASSIGN_OR_RETURN(Type parameter_type,
                           TypeInfoToType(parameter, arena, descriptor_pool));
      overload_decl.mutable_args().push_back(parameter_type);
    }
    CEL_ASSIGN_OR_RETURN(
        Type return_type,
        TypeInfoToType(overload_config.return_type, arena, descriptor_pool));
    overload_decl.set_result(return_type);
    CEL_RETURN_IF_ERROR(function_decl.AddOverload(overload_decl));
  }
  return function_decl;
}

}  // namespace

absl::StatusOr<std::unique_ptr<Compiler>> Env::NewCompiler() {
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(descriptor_pool_, compiler_options_));
  cel::TypeCheckerBuilder& checker_builder =
      compiler_builder->GetCheckerBuilder();

  checker_builder.set_container(config_.GetContainerConfig().name);

  if (!config_.GetStandardLibraryConfig().disable) {
    CEL_RETURN_IF_ERROR(
        compiler_builder->AddLibrary(StandardCompilerLibrary()));
    CEL_ASSIGN_OR_RETURN(CompilerLibrarySubset standard_library_subset,
                         MakeStdlibSubset(config_.GetStandardLibraryConfig()));
    CEL_RETURN_IF_ERROR(
        compiler_builder->AddLibrarySubset(std::move(standard_library_subset)));
  }
  for (const Config::ExtensionConfig& extension_config :
       config_.GetExtensionConfigs()) {
    CEL_ASSIGN_OR_RETURN(CompilerLibrary library,
                         extension_registry_.GetCompilerLibrary(
                             extension_config.name, extension_config.version));
    CEL_RETURN_IF_ERROR(compiler_builder->AddLibrary(std::move(library)));
  }

  google::protobuf::Arena* arena = checker_builder.arena();
  for (const Config::VariableConfig& variable_config :
       config_.GetVariableConfigs()) {
    VariableDecl variable_decl;
    variable_decl.set_name(variable_config.name);
    CEL_ASSIGN_OR_RETURN(Type type,
                         TypeInfoToType(variable_config.type_info, arena,
                                        descriptor_pool_.get()));
    variable_decl.set_type(type);
    if (variable_config.value.has_value()) {
      variable_decl.set_value(variable_config.value);
    }
    CEL_RETURN_IF_ERROR(checker_builder.AddVariable(variable_decl));
  }

  for (const Config::FunctionConfig& function_config :
       config_.GetFunctionConfigs()) {
    CEL_ASSIGN_OR_RETURN(FunctionDecl function_decl,
                         FunctionConfigToFunctionDecl(function_config, arena,
                                                      descriptor_pool_.get()));
    CEL_RETURN_IF_ERROR(checker_builder.AddFunction(function_decl));
  }

  return compiler_builder->Build();
}

}  // namespace cel
