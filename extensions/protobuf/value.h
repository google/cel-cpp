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
//
// Utilities for wrapping and unwrapping cel::Values representing protobuf
// message types.
//
// Handles adapting well-known types to their corresponding CEL representation
// (see https://protobuf.dev/reference/protobuf/google.protobuf/).

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_

#include <type_traits>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_manager.h"
#include "extensions/protobuf/internal/duration_lite.h"
#include "extensions/protobuf/internal/enum.h"
#include "extensions/protobuf/internal/message.h"
#include "extensions/protobuf/internal/struct_lite.h"
#include "extensions/protobuf/internal/timestamp_lite.h"
#include "extensions/protobuf/internal/wrappers_lite.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel::extensions {

// Adapt a protobuf enum value to cel:Value.
template <typename T>
ABSL_DEPRECATED("Use Value::Enum")
std::enable_if_t<protobuf_internal::IsProtoEnum<T>,
                 absl::Status> ProtoEnumToValue(ValueFactory&, T value,
                                                Value& result) {
  result = Value::Enum(value);
  return absl::OkStatus();
}

// Adapt a protobuf enum value to cel:Value.
template <typename T>
ABSL_DEPRECATED("Use Value::Enum")
std::enable_if_t<protobuf_internal::IsProtoEnum<T>,
                 absl::StatusOr<Value>> ProtoEnumToValue(ValueFactory&,
                                                         T value) {
  return Value::Enum(value);
}

// Adapt a cel::Value representing a protobuf enum to the normalized enum value,
// given the enum descriptor.
absl::StatusOr<int> ProtoEnumFromValue(
    const Value& value, absl::Nonnull<const google::protobuf::EnumDescriptor*> desc);

// Adapt a cel::Value representing a protobuf enum to the normalized enum value.
template <typename T>
std::enable_if_t<protobuf_internal::IsProtoEnum<T>, absl::StatusOr<T>>
ProtoEnumFromValue(const Value& value) {
  CEL_ASSIGN_OR_RETURN(
      auto enum_value,
      ProtoEnumFromValue(value, google::protobuf::GetEnumDescriptor<T>()));
  return static_cast<T>(enum_value);
}

// Adapt a protobuf message to a cel::Value.
//
// Handles unwrapping message types with special meanings in CEL (WKTs).
//
// T value must be a protobuf message class.
template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::StatusOr<Value>>
ProtoMessageToValue(ValueManager& value_manager, T&& value) {
  using Tp = std::decay_t<T>;
  if constexpr (std::is_same_v<Tp, google::protobuf::BoolValue>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedBoolValueProto(
                             std::forward<T>(value)));
    return BoolValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::Int32Value>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedInt32ValueProto(
                             std::forward<T>(value)));
    return IntValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::Int64Value>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedInt64ValueProto(
                             std::forward<T>(value)));
    return IntValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::UInt32Value>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedUInt32ValueProto(
                             std::forward<T>(value)));
    return UintValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::UInt64Value>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedUInt64ValueProto(
                             std::forward<T>(value)));
    return UintValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::FloatValue>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedFloatValueProto(
                             std::forward<T>(value)));
    return DoubleValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::DoubleValue>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedDoubleValueProto(
                             std::forward<T>(value)));
    return DoubleValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::BytesValue>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedBytesValueProto(
                             std::forward<T>(value)));
    return BytesValue{std::move(result)};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::StringValue>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedStringValueProto(
                             std::forward<T>(value)));
    return StringValue{std::move(result)};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::Duration>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedDurationProto(
                             std::forward<T>(value)));
    return DurationValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::Timestamp>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::UnwrapGeneratedTimestampProto(
                             std::forward<T>(value)));
    return TimestampValue{result};
  } else if constexpr (std::is_same_v<Tp, google::protobuf::Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result,
        protobuf_internal::GeneratedValueProtoToJson(std::forward<T>(value)));
    return value_manager.CreateValueFromJson(std::move(result));
  } else if constexpr (std::is_same_v<Tp, google::protobuf::ListValue>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::GeneratedListValueProtoToJson(
                             std::forward<T>(value)));
    return value_manager.CreateListValueFromJsonArray(std::move(result));
  } else if constexpr (std::is_same_v<Tp, google::protobuf::Struct>) {
    CEL_ASSIGN_OR_RETURN(
        auto result,
        protobuf_internal::GeneratedStructProtoToJson(std::forward<T>(value)));
    return value_manager.CreateMapValueFromJsonObject(std::move(result));
  } else {
    auto dispatcher = absl::Overload(
        [&](Tp&& m) {
          return protobuf_internal::ProtoMessageToValueImpl(
              value_manager, &m,
              &protobuf_internal::ProtoMessageTraits<Tp>::MoveConstruct);
        },
        [&](const Tp& m) {
          return protobuf_internal::ProtoMessageToValueImpl(
              value_manager, &m,
              &protobuf_internal::ProtoMessageTraits<Tp>::CopyConstruct);
        });
    return dispatcher(std::forward<T>(value));
  }
}

// Adapt a protobuf message to a cel::Value.
//
// Handles unwrapping message types with special meanings in CEL (WKTs).
//
// T value must be a protobuf message class.
template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::Status>
ProtoMessageToValue(ValueManager& value_manager, T&& value,
                    Value& result ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  CEL_ASSIGN_OR_RETURN(
      result, ProtoMessageToValue(value_manager, std::forward<T>(value)));
  return absl::OkStatus();
}

// Extract a protobuf message from a CEL value.
//
// Handles unwrapping message types with special meanings in CEL (WKTs).
//
// T value must be a protobuf message class.
template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::Status>
ProtoMessageFromValue(const Value& value, T& message,
                      absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
                      absl::Nonnull<google::protobuf::MessageFactory*> factory) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, pool, factory,
                                                      &message);
}

// Extract a protobuf message from a CEL value.
//
// Handles unwrapping message types with special meanings in CEL (WKTs).
//
// T value must be a protobuf message class.
template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::Status>
ProtoMessageFromValue(const Value& value, T& message) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, &message);
}

// Extract a protobuf message from a CEL value.
//
// Handles unwrapping message types with special meanings in CEL (WKTs).
//
// T value must be a protobuf message class.
inline absl::StatusOr<absl::Nonnull<google::protobuf::Message*>> ProtoMessageFromValue(
    const Value& value, absl::Nullable<google::protobuf::Arena*> arena) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, arena);
}

// Extract a protobuf message from a CEL value.
//
// Handles unwrapping message types with special meanings in CEL (WKTs).
//
// T value must be a protobuf message class.
inline absl::StatusOr<absl::Nonnull<google::protobuf::Message*>> ProtoMessageFromValue(
    const Value& value, absl::Nullable<google::protobuf::Arena*> arena,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, pool, factory,
                                                      arena);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_
