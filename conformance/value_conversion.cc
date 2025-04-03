// Copyright 2023 Google LLC
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
#include "conformance/value_conversion.h"

#include <string>
#include <utility>

#include "cel/expr/value.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/any.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "extensions/protobuf/value.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"

namespace cel::conformance_internal {
namespace {

using ConformanceKind = cel::expr::Value::KindCase;
using ConformanceMapValue = cel::expr::MapValue;
using ConformanceListValue = cel::expr::ListValue;

std::string ToString(ConformanceKind kind_case) {
  switch (kind_case) {
    case ConformanceKind::kBoolValue:
      return "bool_value";
    case ConformanceKind::kInt64Value:
      return "int64_value";
    case ConformanceKind::kUint64Value:
      return "uint64_value";
    case ConformanceKind::kDoubleValue:
      return "double_value";
    case ConformanceKind::kStringValue:
      return "string_value";
    case ConformanceKind::kBytesValue:
      return "bytes_value";
    case ConformanceKind::kTypeValue:
      return "type_value";
    case ConformanceKind::kEnumValue:
      return "enum_value";
    case ConformanceKind::kMapValue:
      return "map_value";
    case ConformanceKind::kListValue:
      return "list_value";
    case ConformanceKind::kNullValue:
      return "null_value";
    case ConformanceKind::kObjectValue:
      return "object_value";
    default:
      return "unknown kind case";
  }
}

absl::StatusOr<Value> FromObject(
    const google::protobuf::Any& any,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  if (any.type_url() == "type.googleapis.com/google.protobuf.Duration") {
    google::protobuf::Duration duration;
    if (!any.UnpackTo(&duration)) {
      return absl::InvalidArgumentError("invalid duration");
    }
    absl::Duration d = internal::DecodeDuration(duration);
    CEL_RETURN_IF_ERROR(cel::internal::ValidateDuration(d));
    return cel::DurationValue(d);
  } else if (any.type_url() ==
             "type.googleapis.com/google.protobuf.Timestamp") {
    google::protobuf::Timestamp timestamp;
    if (!any.UnpackTo(&timestamp)) {
      return absl::InvalidArgumentError("invalid timestamp");
    }
    absl::Time time = internal::DecodeTime(timestamp);
    CEL_RETURN_IF_ERROR(cel::internal::ValidateTimestamp(time));
    return cel::TimestampValue(time);
  }

  return extensions::ProtoMessageToValue(any, descriptor_pool, message_factory,
                                         arena);
}

absl::StatusOr<MapValue> MapValueFromConformance(
    const ConformanceMapValue& map_value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  auto builder = cel::NewMapValueBuilder(arena);
  for (const auto& entry : map_value.entries()) {
    CEL_ASSIGN_OR_RETURN(auto key,
                         FromConformanceValue(entry.key(), descriptor_pool,
                                              message_factory, arena));
    CEL_ASSIGN_OR_RETURN(auto value,
                         FromConformanceValue(entry.value(), descriptor_pool,
                                              message_factory, arena));
    CEL_RETURN_IF_ERROR(builder->Put(std::move(key), std::move(value)));
  }

  return std::move(*builder).Build();
}

absl::StatusOr<ListValue> ListValueFromConformance(
    const ConformanceListValue& list_value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  auto builder = cel::NewListValueBuilder(arena);
  for (const auto& elem : list_value.values()) {
    CEL_ASSIGN_OR_RETURN(
        auto value,
        FromConformanceValue(elem, descriptor_pool, message_factory, arena));
    CEL_RETURN_IF_ERROR(builder->Add(std::move(value)));
  }

  return std::move(*builder).Build();
}

absl::StatusOr<ConformanceMapValue> MapValueToConformance(
    const MapValue& map_value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  ConformanceMapValue result;

  CEL_ASSIGN_OR_RETURN(auto iter, map_value.NewIterator());

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto key_value,
                         iter->Next(descriptor_pool, message_factory, arena));
    CEL_ASSIGN_OR_RETURN(
        auto value_value,
        map_value.Get(key_value, descriptor_pool, message_factory, arena));

    CEL_ASSIGN_OR_RETURN(
        auto key,
        ToConformanceValue(key_value, descriptor_pool, message_factory, arena));
    CEL_ASSIGN_OR_RETURN(auto value,
                         ToConformanceValue(value_value, descriptor_pool,
                                            message_factory, arena));

    auto* entry = result.add_entries();

    *entry->mutable_key() = std::move(key);
    *entry->mutable_value() = std::move(value);
  }

  return result;
}

absl::StatusOr<ConformanceListValue> ListValueToConformance(
    const ListValue& list_value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  ConformanceListValue result;

  CEL_ASSIGN_OR_RETURN(auto iter, list_value.NewIterator());

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto elem,
                         iter->Next(descriptor_pool, message_factory, arena));
    CEL_ASSIGN_OR_RETURN(
        *result.add_values(),
        ToConformanceValue(elem, descriptor_pool, message_factory, arena));
  }

  return result;
}

absl::StatusOr<google::protobuf::Any> ToProtobufAny(
    const StructValue& struct_value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  google::protobuf::io::CordOutputStream serialized;
  CEL_RETURN_IF_ERROR(
      struct_value.SerializeTo(descriptor_pool, message_factory, &serialized));
  google::protobuf::Any result;
  result.set_type_url(MakeTypeUrl(struct_value.GetTypeName()));
  result.set_value(std::string(std::move(serialized).Consume()));

  return result;
}

}  // namespace

absl::StatusOr<Value> FromConformanceValue(
    const cel::expr::Value& value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  google::protobuf::LinkMessageReflection<cel::expr::Value>();
  switch (value.kind_case()) {
    case ConformanceKind::kBoolValue:
      return cel::BoolValue(value.bool_value());
    case ConformanceKind::kInt64Value:
      return cel::IntValue(value.int64_value());
    case ConformanceKind::kUint64Value:
      return cel::UintValue(value.uint64_value());
    case ConformanceKind::kDoubleValue:
      return cel::DoubleValue(value.double_value());
    case ConformanceKind::kStringValue:
      return cel::StringValue(value.string_value());
    case ConformanceKind::kBytesValue:
      return cel::BytesValue(value.bytes_value());
    case ConformanceKind::kNullValue:
      return cel::NullValue();
    case ConformanceKind::kObjectValue:
      return FromObject(value.object_value(), descriptor_pool, message_factory,
                        arena);
    case ConformanceKind::kMapValue:
      return MapValueFromConformance(value.map_value(), descriptor_pool,
                                     message_factory, arena);
    case ConformanceKind::kListValue:
      return ListValueFromConformance(value.list_value(), descriptor_pool,
                                      message_factory, arena);

    default:
      return absl::UnimplementedError(absl::StrCat(
          "FromConformanceValue not supported ", ToString(value.kind_case())));
  }
}

absl::StatusOr<cel::expr::Value> ToConformanceValue(
    const Value& value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) {
  cel::expr::Value result;
  switch (value->kind()) {
    case ValueKind::kBool:
      result.set_bool_value(value.GetBool().NativeValue());
      break;
    case ValueKind::kInt:
      result.set_int64_value(value.GetInt().NativeValue());
      break;
    case ValueKind::kUint:
      result.set_uint64_value(value.GetUint().NativeValue());
      break;
    case ValueKind::kDouble:
      result.set_double_value(value.GetDouble().NativeValue());
      break;
    case ValueKind::kString:
      result.set_string_value(value.GetString().ToString());
      break;
    case ValueKind::kBytes:
      result.set_bytes_value(value.GetBytes().ToString());
      break;
    case ValueKind::kType:
      result.set_type_value(value.GetType().name());
      break;
    case ValueKind::kNull:
      result.set_null_value(google::protobuf::NullValue::NULL_VALUE);
      break;
    case ValueKind::kDuration: {
      google::protobuf::Duration duration;
      CEL_RETURN_IF_ERROR(internal::EncodeDuration(
          value.GetDuration().NativeValue(), &duration));
      result.mutable_object_value()->PackFrom(duration);
      break;
    }
    case ValueKind::kTimestamp: {
      google::protobuf::Timestamp timestamp;
      CEL_RETURN_IF_ERROR(
          internal::EncodeTime(value.GetTimestamp().NativeValue(), &timestamp));
      result.mutable_object_value()->PackFrom(timestamp);
      break;
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(
          *result.mutable_map_value(),
          MapValueToConformance(value.GetMap(), descriptor_pool,
                                message_factory, arena));
      break;
    }
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(
          *result.mutable_list_value(),
          ListValueToConformance(value.GetList(), descriptor_pool,
                                 message_factory, arena));
      break;
    }
    case ValueKind::kStruct: {
      CEL_ASSIGN_OR_RETURN(*result.mutable_object_value(),
                           ToProtobufAny(value.GetStruct(), descriptor_pool,
                                         message_factory, arena));
      break;
    }
    default:
      return absl::UnimplementedError(
          absl::StrCat("ToConformanceValue not supported ",
                       ValueKindToString(value->kind())));
  }
  return result;
}

}  // namespace cel::conformance_internal
