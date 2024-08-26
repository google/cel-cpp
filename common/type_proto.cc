// Copyright 2024 Google LLC
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

#include "common/type_proto.h"

#include <cstddef>

#include "cel/expr/checked.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/type_pool.h"

namespace cel {

namespace {

using TypeProto = ::cel::expr::Type;
using ListTypeProto = typename TypeProto::ListType;
using MapTypeProto = typename TypeProto::MapType;
using FunctionTypeProto = typename TypeProto::FunctionType;
using OpaqueTypeProto = typename TypeProto::AbstractType;
using PrimitiveTypeProto = typename TypeProto::PrimitiveType;
using WellKnownTypeProto = typename TypeProto::WellKnownType;

struct TypeFromProtoConverter final {
  explicit TypeFromProtoConverter(absl::Nonnull<TypePool*> type_pool)
      : type_pool(type_pool) {}

  absl::optional<Type> FromType(const TypeProto& proto) {
    switch (proto.type_kind_case()) {
      case TypeProto::TYPE_KIND_NOT_SET:
        ABSL_FALLTHROUGH_INTENDED;
      case TypeProto::kDyn:
        return DynType();
      case TypeProto::kNull:
        return NullType();
      case TypeProto::kPrimitive:
        switch (proto.primitive()) {
          case TypeProto::BOOL:
            return BoolType();
          case TypeProto::INT64:
            return IntType();
          case TypeProto::UINT64:
            return UintType();
          case TypeProto::DOUBLE:
            return DoubleType();
          case TypeProto::STRING:
            return StringType();
          case TypeProto::BYTES:
            return BytesType();
          default:
            status = absl::DataLossError(absl::StrCat(
                "unexpected primitive type kind: ", proto.primitive()));
            return absl::nullopt;
        }
      case TypeProto::kWrapper:
        switch (proto.wrapper()) {
          case TypeProto::BOOL:
            return BoolWrapperType();
          case TypeProto::INT64:
            return IntWrapperType();
          case TypeProto::UINT64:
            return UintWrapperType();
          case TypeProto::DOUBLE:
            return DoubleWrapperType();
          case TypeProto::STRING:
            return StringWrapperType();
          case TypeProto::BYTES:
            return BytesWrapperType();
          default:
            status = absl::DataLossError(absl::StrCat(
                "unexpected wrapper type kind: ", proto.wrapper()));
            return absl::nullopt;
        }
      case TypeProto::kWellKnown:
        switch (proto.well_known()) {
          case TypeProto::ANY:
            return AnyType();
          case TypeProto::DURATION:
            return DurationType();
          case TypeProto::TIMESTAMP:
            return TimestampType();
          default:
            status = absl::DataLossError(absl::StrCat(
                "unexpected well known type kind: ", proto.well_known()));
            return absl::nullopt;
        }
      case TypeProto::kListType: {
        auto elem = FromType(proto.list_type().elem_type());
        if (ABSL_PREDICT_FALSE(!elem.has_value())) {
          ABSL_DCHECK(!status.ok());
          return absl::nullopt;
        }
        return type_pool->MakeListType(*elem);
      }
      case TypeProto::kMapType: {
        auto key = FromType(proto.map_type().key_type());
        if (ABSL_PREDICT_FALSE(!key.has_value())) {
          ABSL_DCHECK(!status.ok());
          return absl::nullopt;
        }
        auto value = FromType(proto.map_type().value_type());
        if (ABSL_PREDICT_FALSE(!value.has_value())) {
          ABSL_DCHECK(!status.ok());
          return absl::nullopt;
        }
        return type_pool->MakeMapType(*key, *value);
      }
      case TypeProto::kFunction: {
        auto result = FromType(proto.function().result_type());
        if (ABSL_PREDICT_FALSE(!result.has_value())) {
          ABSL_DCHECK(!status.ok());
          return absl::nullopt;
        }
        absl::InlinedVector<Type, 3> args;
        args.reserve(static_cast<size_t>(proto.function().arg_types().size()));
        for (const auto& arg_proto : proto.function().arg_types()) {
          auto arg = FromType(arg_proto);
          if (ABSL_PREDICT_FALSE(!arg.has_value())) {
            ABSL_DCHECK(!status.ok());
            return absl::nullopt;
          }
          args.push_back(*arg);
        }
        return type_pool->MakeFunctionType(*result, args);
      }
      case TypeProto::kMessageType:
        if (ABSL_PREDICT_FALSE(proto.message_type().empty())) {
          status =
              absl::InvalidArgumentError("unexpected empty message type name");
          return absl::nullopt;
        }
        if (ABSL_PREDICT_FALSE(IsWellKnownMessageType(proto.message_type()))) {
          status = absl::InvalidArgumentError(
              absl::StrCat("well known type masquerading as message type: ",
                           proto.message_type()));
          return absl::nullopt;
        }
        return type_pool->MakeStructType(proto.message_type());
      case TypeProto::kTypeParam:
        if (ABSL_PREDICT_FALSE(proto.type_param().empty())) {
          status =
              absl::InvalidArgumentError("unexpected empty type param name");
          return absl::nullopt;
        }
        return type_pool->MakeTypeParamType(proto.type_param());
      case TypeProto::kType: {
        auto type = FromType(proto.type());
        if (ABSL_PREDICT_FALSE(!type.has_value())) {
          ABSL_DCHECK(!status.ok());
          return absl::nullopt;
        }
        return type_pool->MakeTypeType(*type);
      }
      case TypeProto::kError:
        return ErrorType();
      case TypeProto::kAbstractType: {
        if (proto.abstract_type().name().empty()) {
          status =
              absl::InvalidArgumentError("unexpected empty opaque type name");
          return absl::nullopt;
        }
        absl::InlinedVector<Type, 3> params;
        params.reserve(static_cast<size_t>(
            proto.abstract_type().parameter_types().size()));
        for (const auto& param_proto :
             proto.abstract_type().parameter_types()) {
          auto param = FromType(param_proto);
          if (ABSL_PREDICT_FALSE(!param.has_value())) {
            ABSL_DCHECK(!status.ok());
            return absl::nullopt;
          }
          params.push_back(*param);
        }
        return type_pool->MakeOpaqueType(proto.abstract_type().name(), params);
      }
      default:
        status = absl::DataLossError(absl::StrCat("unexpected type kind case: ",
                                                  proto.type_kind_case()));
        return absl::nullopt;
    }
  }

  absl::Nonnull<TypePool*> const type_pool;
  absl::Status status;
};

}  // namespace

absl::StatusOr<Type> TypeFromProto(absl::Nonnull<TypePool*> type_pool,
                                   const TypeProto& proto) {
  TypeFromProtoConverter converter(type_pool);
  auto type = converter.FromType(proto);
  if (ABSL_PREDICT_FALSE(!type.has_value())) {
    ABSL_DCHECK(!converter.status.ok());
    return converter.status;
  }
  return *type;
}

namespace {

struct TypeToProtoConverter final {
  bool FromType(const Type& type, absl::Nonnull<TypeProto*> proto) {
    switch (type.kind()) {
      case TypeKind::kDyn:
        proto->mutable_dyn();
        return true;
      case TypeKind::kNull:
        proto->set_null(google::protobuf::NULL_VALUE);
        return true;
      case TypeKind::kBool:
        proto->set_primitive(TypeProto::BOOL);
        return true;
      case TypeKind::kInt:
        proto->set_primitive(TypeProto::INT64);
        return true;
      case TypeKind::kUint:
        proto->set_primitive(TypeProto::UINT64);
        return true;
      case TypeKind::kDouble:
        proto->set_primitive(TypeProto::DOUBLE);
        return true;
      case TypeKind::kBytes:
        proto->set_primitive(TypeProto::BYTES);
        return true;
      case TypeKind::kString:
        proto->set_primitive(TypeProto::STRING);
        return true;
      case TypeKind::kBoolWrapper:
        proto->set_wrapper(TypeProto::BOOL);
        return true;
      case TypeKind::kIntWrapper:
        proto->set_wrapper(TypeProto::INT64);
        return true;
      case TypeKind::kUintWrapper:
        proto->set_wrapper(TypeProto::UINT64);
        return true;
      case TypeKind::kDoubleWrapper:
        proto->set_wrapper(TypeProto::DOUBLE);
        return true;
      case TypeKind::kBytesWrapper:
        proto->set_wrapper(TypeProto::BYTES);
        return true;
      case TypeKind::kStringWrapper:
        proto->set_wrapper(TypeProto::STRING);
        return true;
      case TypeKind::kAny:
        proto->set_well_known(TypeProto::ANY);
        return true;
      case TypeKind::kDuration:
        proto->set_well_known(TypeProto::DURATION);
        return true;
      case TypeKind::kTimestamp:
        proto->set_well_known(TypeProto::TIMESTAMP);
        return true;
      case TypeKind::kList:
        return FromType(static_cast<ListType>(type).GetElement(),
                        proto->mutable_list_type()->mutable_elem_type());
      case TypeKind::kMap:
        return FromType(static_cast<MapType>(type).GetKey(),
                        proto->mutable_map_type()->mutable_key_type()) &&
               FromType(static_cast<MapType>(type).GetValue(),
                        proto->mutable_map_type()->mutable_value_type());
      case TypeKind::kStruct:
        proto->set_message_type(static_cast<StructType>(type).name());
        return true;
      case TypeKind::kOpaque: {
        auto opaque_type = static_cast<OpaqueType>(type);
        auto* opaque_type_proto = proto->mutable_abstract_type();
        opaque_type_proto->set_name(opaque_type.name());
        auto opaque_type_params = opaque_type.GetParameters();
        opaque_type_proto->mutable_parameter_types()->Reserve(
            static_cast<int>(opaque_type_params.size()));
        for (const auto& param : opaque_type_params) {
          if (ABSL_PREDICT_FALSE(
                  !FromType(param, opaque_type_proto->add_parameter_types()))) {
            return false;
          }
        }
        return true;
      }
      case TypeKind::kTypeParam:
        proto->set_type_param(static_cast<TypeParamType>(type).name());
        return true;
      case TypeKind::kType:
        return FromType(static_cast<TypeType>(type).GetType(),
                        proto->mutable_type());
      case TypeKind::kFunction: {
        auto function_type = static_cast<FunctionType>(type);
        auto* function_type_proto = proto->mutable_function();
        if (ABSL_PREDICT_FALSE(
                !FromType(function_type.result(),
                          function_type_proto->mutable_result_type()))) {
          return false;
        }
        auto function_type_args = function_type.args();
        function_type_proto->mutable_arg_types()->Reserve(
            static_cast<int>(function_type_args.size()));
        for (const auto& arg : function_type_args) {
          if (ABSL_PREDICT_FALSE(
                  !FromType(arg, function_type_proto->add_arg_types()))) {
            return false;
          }
        }
        return true;
      }
      case TypeKind::kError:
        proto->mutable_error();
        return true;
      default:
        status = absl::DataLossError(
            absl::StrCat("unexpected type kind: ", type.kind()));
        return false;
    }
  }

  absl::Status status;
};

}  // namespace

absl::Status TypeToProto(const Type& type, absl::Nonnull<TypeProto*> proto) {
  TypeToProtoConverter converter;
  if (ABSL_PREDICT_FALSE(!converter.FromType(type, proto))) {
    ABSL_DCHECK(!converter.status.ok());
    return converter.status;
  }
  return absl::OkStatus();
}

}  // namespace cel
