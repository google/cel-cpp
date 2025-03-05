// Copyright 2025 Google LLC
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
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

namespace {

// filter well-known types from message types.
absl::optional<Type> MaybeWellKnownType(absl::string_view type_name) {
  static const absl::flat_hash_map<absl::string_view, Type>* kWellKnownTypes =
      []() {
        auto* instance = new absl::flat_hash_map<absl::string_view, Type>{
            // keep-sorted start
            {"google.protobuf.Any", AnyType()},
            {"google.protobuf.BoolValue", BoolWrapperType()},
            {"google.protobuf.BytesValue", BytesWrapperType()},
            {"google.protobuf.DoubleValue", DoubleWrapperType()},
            {"google.protobuf.Duration", DurationType()},
            {"google.protobuf.FloatValue", DoubleWrapperType()},
            {"google.protobuf.Int32Value", IntWrapperType()},
            {"google.protobuf.Int64Value", IntWrapperType()},
            {"google.protobuf.ListValue", ListType()},
            {"google.protobuf.StringValue", StringWrapperType()},
            {"google.protobuf.Struct", JsonMapType()},
            {"google.protobuf.Timestamp", TimestampType()},
            {"google.protobuf.UInt32Value", UintWrapperType()},
            {"google.protobuf.UInt64Value", UintWrapperType()},
            {"google.protobuf.Value", DynType()},
            // keep-sorted end
        };
        return instance;
      }();

  if (auto it = kWellKnownTypes->find(type_name);
      it != kWellKnownTypes->end()) {
    return it->second;
  }

  return absl::nullopt;
}

}  // namespace

using TypePb = cel::expr::Type;

absl::StatusOr<Type> TypeFromProto(
    const cel::expr::Type& type_pb,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  switch (type_pb.type_kind_case()) {
    case TypePb::kAbstractType: {
      auto* name = google::protobuf::Arena::Create<std::string>(
          arena, type_pb.abstract_type().name());
      std::vector<Type> params;
      params.resize(type_pb.abstract_type().parameter_types_size());
      size_t i = 0;
      for (const auto& p : type_pb.abstract_type().parameter_types()) {
        CEL_ASSIGN_OR_RETURN(params[i],
                             TypeFromProto(p, descriptor_pool, arena));
        i++;
      }
      return OpaqueType(arena, *name, params);
    }
    case TypePb::kDyn:
      return DynType();
    case TypePb::kError:
      return ErrorType();
    case TypePb::kListType: {
      CEL_ASSIGN_OR_RETURN(Type element,
                           TypeFromProto(type_pb.list_type().elem_type(),
                                         descriptor_pool, arena));
      return ListType(arena, element);
    }
    case TypePb::kMapType: {
      CEL_ASSIGN_OR_RETURN(
          Type key,
          TypeFromProto(type_pb.map_type().key_type(), descriptor_pool, arena));
      CEL_ASSIGN_OR_RETURN(Type value,
                           TypeFromProto(type_pb.map_type().value_type(),
                                         descriptor_pool, arena));
      return MapType(arena, key, value);
    }
    case TypePb::kMessageType: {
      if (auto well_known = MaybeWellKnownType(type_pb.message_type());
          well_known.has_value()) {
        return *well_known;
      }

      const auto* descriptor =
          descriptor_pool->FindMessageTypeByName(type_pb.message_type());
      if (descriptor == nullptr) {
        return absl::InvalidArgumentError(
            absl::StrCat("unknown message type: ", type_pb.message_type()));
      }
      return MessageType(descriptor);
    }
    case TypePb::kNull:
      return NullType();
    case TypePb::kPrimitive:
      switch (type_pb.primitive()) {
        case TypePb::BOOL:
          return BoolType();
        case TypePb::BYTES:
          return BytesType();
        case TypePb::DOUBLE:
          return DoubleType();
        case TypePb::INT64:
          return IntType();
        case TypePb::STRING:
          return StringType();
        case TypePb::UINT64:
          return UintType();
        default:
          return absl::InvalidArgumentError("unknown primitive kind");
      }
    case TypePb::kType: {
      CEL_ASSIGN_OR_RETURN(
          Type nested, TypeFromProto(type_pb.type(), descriptor_pool, arena));
      return TypeType(arena, nested);
    }
    case TypePb::kTypeParam: {
      auto* name =
          google::protobuf::Arena::Create<std::string>(arena, type_pb.type_param());
      return TypeParamType(*name);
    }
    case TypePb::kWellKnown:
      switch (type_pb.well_known()) {
        case TypePb::ANY:
          return AnyType();
        case TypePb::DURATION:
          return DurationType();
        case TypePb::TIMESTAMP:
          return TimestampType();
        default:
          break;
      }
      return absl::InvalidArgumentError("unknown well known type.");
    case TypePb::kWrapper: {
      switch (type_pb.wrapper()) {
        case TypePb::BOOL:
          return BoolWrapperType();
        case TypePb::BYTES:
          return BytesWrapperType();
        case TypePb::DOUBLE:
          return DoubleWrapperType();
        case TypePb::INT64:
          return IntWrapperType();
        case TypePb::STRING:
          return StringWrapperType();
        case TypePb::UINT64:
          return UintWrapperType();
        default:
          return absl::InvalidArgumentError("unknown primitive wrapper kind");
      }
    }
    // Function types are not supported in the C++ type checker.
    case TypePb::kFunction:
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unsupported type kind: ", type_pb.type_kind_case()));
  }
}

}  // namespace cel
