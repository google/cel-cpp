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

#include "env/type_info.h"

#include <optional>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "env/config.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

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
          {"wrapper<bool>", TypeKind::kBoolWrapper},
          {IntWrapperType::kName, TypeKind::kIntWrapper},
          {"wrapper<int>", TypeKind::kIntWrapper},
          {UintWrapperType::kName, TypeKind::kUintWrapper},
          {"wrapper<uint>", TypeKind::kUintWrapper},
          {DoubleWrapperType::kName, TypeKind::kDoubleWrapper},
          {"wrapper<double>", TypeKind::kDoubleWrapper},
          {StringWrapperType::kName, TypeKind::kStringWrapper},
          {"wrapper<string>", TypeKind::kStringWrapper},
          {BytesWrapperType::kName, TypeKind::kBytesWrapper},
          {"wrapper<bytes>", TypeKind::kBytesWrapper},
          {"type", TypeKind::kType},
      });
  if (auto it = kTypeNameToTypeKind->find(type_name);
      it != kTypeNameToTypeKind->end()) {
    return it->second;
  }

  return std::nullopt;
}
}  // namespace

absl::StatusOr<Type> TypeInfoToType(
    const Config::TypeInfo& type_info,
    const google::protobuf::DescriptorPool* descriptor_pool, google::protobuf::Arena* arena) {
  if (type_info.is_type_param) {
    return TypeParamType(type_info.name);
  }

  std::optional<TypeKind> type_kind = TypeNameToTypeKind(type_info.name);
  if (!type_kind.has_value()) {
    if (type_info.params.empty() && descriptor_pool != nullptr) {
      const google::protobuf::Descriptor* type =
          descriptor_pool->FindMessageTypeByName(type_info.name);
      if (type != nullptr) {
        return Type::Message(type);
      }
    }
    // TODO(uncreated-issue/88): use a TypeIntrospector to validate opaque types
    std::vector<Type> parameter_types;
    for (const Config::TypeInfo& param : type_info.params) {
      CEL_ASSIGN_OR_RETURN(Type parameter_type,
                           TypeInfoToType(param, descriptor_pool, arena));
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
            TypeInfoToType(type_info.params[0], descriptor_pool, arena));
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
                                                      descriptor_pool, arena));
      }
      if (type_info.params.size() > 1) {
        CEL_ASSIGN_OR_RETURN(
            value_type,
            TypeInfoToType(type_info.params[1], descriptor_pool, arena));
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
      CEL_ASSIGN_OR_RETURN(Type type, TypeInfoToType(type_info.params[0],
                                                     descriptor_pool, arena));
      return TypeType(arena, type);
    }
    default:
      return DynType();
  }
}

}  // namespace cel
