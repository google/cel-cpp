// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/type_spec_resolver.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "internal/status_macros.h"

namespace cel {

absl::StatusOr<Type> ConvertTypeSpecToType(const TypeSpec& type_spec,
                                           google::protobuf::Arena* arena,
                                           const google::protobuf::DescriptorPool& pool) {
  if (type_spec.has_null()) return Type(NullType{});
  if (type_spec.has_dyn()) return Type(DynType{});

  if (type_spec.has_primitive()) {
    switch (type_spec.primitive()) {
      case PrimitiveType::kBool:
        return Type(BoolType{});
      case PrimitiveType::kInt64:
        return Type(IntType{});
      case PrimitiveType::kUint64:
        return Type(UintType{});
      case PrimitiveType::kDouble:
        return Type(DoubleType{});
      case PrimitiveType::kString:
        return Type(StringType{});
      case PrimitiveType::kBytes:
        return Type(BytesType{});
      default:
        return absl::InvalidArgumentError("Unsupported primitive type");
    }
  }

  if (type_spec.has_well_known()) {
    switch (type_spec.well_known()) {
      case WellKnownTypeSpec::kAny:
        return Type(AnyType{});
      case WellKnownTypeSpec::kTimestamp:
        return Type(TimestampType{});
      case WellKnownTypeSpec::kDuration:
        return Type(DurationType{});
      default:
        return absl::InvalidArgumentError("Unsupported well-known type");
    }
  }

  if (type_spec.has_wrapper()) {
    switch (type_spec.wrapper()) {
      case PrimitiveType::kBool:
        return Type(BoolWrapperType{});
      case PrimitiveType::kInt64:
        return Type(IntWrapperType{});
      case PrimitiveType::kUint64:
        return Type(UintWrapperType{});
      case PrimitiveType::kDouble:
        return Type(DoubleWrapperType{});
      case PrimitiveType::kString:
        return Type(StringWrapperType{});
      case PrimitiveType::kBytes:
        return Type(BytesWrapperType{});
      default:
        return absl::InvalidArgumentError("Unsupported wrapper type");
    }
  }

  if (type_spec.has_list_type()) {
    CEL_ASSIGN_OR_RETURN(
        auto elem_type,
        ConvertTypeSpecToType(type_spec.list_type().elem_type(), arena, pool));
    return Type(ListType(arena, elem_type));
  }

  if (type_spec.has_map_type()) {
    CEL_ASSIGN_OR_RETURN(
        auto key_type,
        ConvertTypeSpecToType(type_spec.map_type().key_type(), arena, pool));
    CEL_ASSIGN_OR_RETURN(
        auto value_type,
        ConvertTypeSpecToType(type_spec.map_type().value_type(), arena, pool));
    return Type(MapType(arena, key_type, value_type));
  }

  if (type_spec.has_function()) {
    const auto& func_spec = type_spec.function();
    CEL_ASSIGN_OR_RETURN(
        auto result_type,
        ConvertTypeSpecToType(func_spec.result_type(), arena, pool));
    std::vector<Type> arg_types;
    for (const auto& arg_spec : func_spec.arg_types()) {
      CEL_ASSIGN_OR_RETURN(auto arg_type,
                           ConvertTypeSpecToType(arg_spec, arena, pool));
      arg_types.push_back(std::move(arg_type));
    }
    return Type(FunctionType(arena, result_type, arg_types));
  }

  if (type_spec.has_type_param()) {
    const std::string& name = type_spec.type_param().type();
    auto* allocated_name = google::protobuf::Arena::Create<std::string>(arena, name);
    return Type(TypeParamType(absl::string_view(*allocated_name)));
  }

  if (type_spec.has_message_type()) {
    const std::string& name = type_spec.message_type().type();
    const google::protobuf::Descriptor* descriptor = pool.FindMessageTypeByName(name);
    if (descriptor == nullptr) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Message type '", name, "' not found in descriptor pool"));
    }
    return Type::Message(descriptor);
  }

  if (type_spec.has_abstract_type()) {
    const std::string& name = type_spec.abstract_type().name();

    // Check if it's a message type in the pool
    const google::protobuf::Descriptor* descriptor = pool.FindMessageTypeByName(name);
    if (descriptor != nullptr) {
      if (!type_spec.abstract_type().parameter_types().empty()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Message type '", name, "' cannot have type parameters"));
      }
      return Type::Message(descriptor);
    }

    // Check if it's an enum type in the pool
    const google::protobuf::EnumDescriptor* enum_descriptor =
        pool.FindEnumTypeByName(name);
    if (enum_descriptor != nullptr) {
      if (!type_spec.abstract_type().parameter_types().empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Enum type '", name, "' cannot have type parameters"));
      }
      return Type::Enum(enum_descriptor);
    }

    // Otherwise fallback to OpaqueType
    std::vector<Type> params;
    for (const auto& param_spec : type_spec.abstract_type().parameter_types()) {
      CEL_ASSIGN_OR_RETURN(auto param,
                           ConvertTypeSpecToType(param_spec, arena, pool));
      params.push_back(std::move(param));
    }
    auto* allocated_name = google::protobuf::Arena::Create<std::string>(arena, name);
    return Type(OpaqueType(arena, absl::string_view(*allocated_name), params));
  }

  if (type_spec.has_type()) {
    CEL_ASSIGN_OR_RETURN(auto contained_type,
                         ConvertTypeSpecToType(type_spec.type(), arena, pool));
    return Type(TypeType(arena, contained_type));
  }

  if (type_spec.has_error()) {
    return Type(ErrorType{});
  }

  return absl::InvalidArgumentError("Unknown TypeSpec kind");
}

}  // namespace cel
