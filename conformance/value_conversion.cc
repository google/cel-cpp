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

#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/duration_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value_builder.h"
#include "base/values/map_value.h"
#include "base/values/map_value_builder.h"
#include "base/values/timestamp_value.h"
#include "base/values/type_value.h"
#include "base/values/uint_value.h"
#include "common/any.h"
#include "extensions/protobuf/struct_value.h"
#include "extensions/protobuf/value.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"
#include "google/protobuf/message.h"

namespace cel::conformance_internal {
namespace {

using ConformanceKind = google::api::expr::v1alpha1::Value::KindCase;
using ConformanceMapValue = google::api::expr::v1alpha1::MapValue;
using ConformanceListValue = google::api::expr::v1alpha1::ListValue;

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

absl::StatusOr<Handle<Value>> FromObject(ValueFactory& value_factory,
                                         const google::protobuf::Any& any) {
  if (any.type_url() == "type.googleapis.com/google.protobuf.Duration") {
    google::protobuf::Duration duration;
    if (!any.UnpackTo(&duration)) {
      return absl::InvalidArgumentError("invalid duration");
    }
    return value_factory.CreateDurationValue(
        internal::DecodeDuration(duration));
  } else if (any.type_url() ==
             "type.googleapis.com/google.protobuf.Timestamp") {
    google::protobuf::Timestamp timestamp;
    if (!any.UnpackTo(&timestamp)) {
      return absl::InvalidArgumentError("invalid timestamp");
    }
    return value_factory.CreateTimestampValue(internal::DecodeTime(timestamp));
  }

  return extensions::ProtoValue::Create(value_factory, any);
}

absl::StatusOr<Handle<MapValue>> MapValueFromConformance(
    ValueFactory& value_factory, const ConformanceMapValue& map_value) {
  MapValueBuilder<Value, Value> builder(
      value_factory, value_factory.type_factory().GetDynType(),
      value_factory.type_factory().GetDynType());
  for (const auto& entry : map_value.entries()) {
    CEL_ASSIGN_OR_RETURN(auto key,
                         FromConformanceValue(value_factory, entry.key()));
    CEL_ASSIGN_OR_RETURN(auto value,
                         FromConformanceValue(value_factory, entry.value()));
    CEL_RETURN_IF_ERROR(builder.Put(std::move(key), std::move(value)));
  }

  return std::move(builder).Build();
}

absl::StatusOr<Handle<ListValue>> ListValueFromConformance(
    ValueFactory& value_factory, const ConformanceListValue& list_value) {
  ListValueBuilder<Value> builder(value_factory,
                                  value_factory.type_factory().GetDynType());
  for (const auto& elem : list_value.values()) {
    CEL_ASSIGN_OR_RETURN(auto value, FromConformanceValue(value_factory, elem));
    CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
  }

  return std::move(builder).Build();
}

absl::StatusOr<ConformanceMapValue> MapValueToConformance(
    ValueFactory& value_factory, const MapValue& map_value) {
  ConformanceMapValue result;

  CEL_ASSIGN_OR_RETURN(auto iter, map_value.NewIterator(value_factory));

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto key_value, iter->Next());
    CEL_ASSIGN_OR_RETURN(auto value_value,
                         map_value.Get(value_factory, key_value));

    CEL_ASSIGN_OR_RETURN(auto key,
                         ToConformanceValue(value_factory, key_value));
    CEL_ASSIGN_OR_RETURN(auto value,
                         ToConformanceValue(value_factory, value_value));

    auto* entry = result.add_entries();

    *entry->mutable_key() = std::move(key);
    *entry->mutable_value() = std::move(value);
  }

  return result;
}

absl::StatusOr<ConformanceListValue> ListValueToConformance(
    ValueFactory& value_factory, const ListValue& list_value) {
  ConformanceListValue result;

  CEL_ASSIGN_OR_RETURN(auto iter, list_value.NewIterator(value_factory));

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto elem, iter->Next());
    CEL_ASSIGN_OR_RETURN(*result.add_values(),
                         ToConformanceValue(value_factory, elem));
  }

  return result;
}

absl::StatusOr<google::protobuf::Any> ToProtobufAny(
    ValueFactory& value_factory, const StructValue& struct_value) {
  CEL_ASSIGN_OR_RETURN(cel::Any any, struct_value.ConvertToAny(value_factory));

  google::protobuf::Any result;
  result.set_type_url(any.type_url());
  result.set_value(std::string(any.value()));

  return result;
}
}  // namespace

absl::StatusOr<Handle<Value>> FromConformanceValue(
    ValueFactory& value_factory, const google::api::expr::v1alpha1::Value& value) {
  google::protobuf::LinkMessageReflection<google::api::expr::v1alpha1::Value>();
  switch (value.kind_case()) {
    case ConformanceKind::kBoolValue:
      return value_factory.CreateBoolValue(value.bool_value());
    case ConformanceKind::kInt64Value:
      return value_factory.CreateIntValue(value.int64_value());
    case ConformanceKind::kUint64Value:
      return value_factory.CreateUintValue(value.uint64_value());
    case ConformanceKind::kDoubleValue:
      return value_factory.CreateDoubleValue(value.double_value());
    case ConformanceKind::kStringValue:
      return value_factory.CreateStringValue(value.string_value());
    case ConformanceKind::kBytesValue:
      return value_factory.CreateBytesValue(value.bytes_value());
    case ConformanceKind::kNullValue:
      return value_factory.GetNullValue();
    case ConformanceKind::kObjectValue:
      return FromObject(value_factory, value.object_value());
    case ConformanceKind::kMapValue:
      return MapValueFromConformance(value_factory, value.map_value());
    case ConformanceKind::kListValue:
      return ListValueFromConformance(value_factory, value.list_value());

    default:
      return absl::UnimplementedError(absl::StrCat(
          "FromConformanceValue not supported ", ToString(value.kind_case())));
  }
}

absl::StatusOr<google::api::expr::v1alpha1::Value> ToConformanceValue(
    ValueFactory& value_factory, const Handle<Value>& value) {
  google::api::expr::v1alpha1::Value result;
  switch (value->kind()) {
    case ValueKind::kBool:
      result.set_bool_value(value->As<BoolValue>().value());
      break;
    case ValueKind::kInt:
      result.set_int64_value(value->As<IntValue>().value());
      break;
    case ValueKind::kUint:
      result.set_uint64_value(value->As<UintValue>().value());
      break;
    case ValueKind::kDouble:
      result.set_double_value(value->As<DoubleValue>().value());
      break;
    case ValueKind::kString:
      result.set_string_value(value->As<StringValue>().ToString());
      break;
    case ValueKind::kBytes:
      result.set_bytes_value(value->As<BytesValue>().ToString());
      break;
    case ValueKind::kType:
      result.set_type_value(value->As<TypeValue>().name());
      break;
    case ValueKind::kNull:
      result.set_null_value(google::protobuf::NullValue::NULL_VALUE);
      break;
    case ValueKind::kDuration: {
      google::protobuf::Duration duration;
      CEL_RETURN_IF_ERROR(internal::EncodeDuration(
          value->As<DurationValue>().value(), &duration));
      result.mutable_object_value()->PackFrom(duration);
      break;
    }
    case ValueKind::kTimestamp: {
      google::protobuf::Timestamp timestamp;
      CEL_RETURN_IF_ERROR(internal::EncodeTime(
          value->As<TimestampValue>().value(), &timestamp));
      result.mutable_object_value()->PackFrom(timestamp);
      break;
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(
          *result.mutable_map_value(),
          MapValueToConformance(value_factory, value->As<MapValue>()));
      break;
    }
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(
          *result.mutable_list_value(),
          ListValueToConformance(value_factory, value->As<ListValue>()));
      break;
    }
    case ValueKind::kStruct: {
      CEL_ASSIGN_OR_RETURN(
          *result.mutable_object_value(),
          ToProtobufAny(value_factory, value->As<StructValue>()));
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
