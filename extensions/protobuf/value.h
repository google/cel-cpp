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
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_manager.h"
#include "extensions/protobuf/internal/duration.h"
#include "extensions/protobuf/internal/enum.h"
#include "extensions/protobuf/internal/message.h"
#include "extensions/protobuf/internal/struct.h"
#include "extensions/protobuf/internal/timestamp.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoEnum<T>, absl::StatusOr<ValueView>>
ProtoEnumToValue(ValueFactory&, T value,
                 Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND
                     ABSL_ATTRIBUTE_UNUSED) {
  if constexpr (std::is_same_v<T, google::protobuf::NullValue>) {
    return NullValueView{};
  }
  return IntValueView{static_cast<int>(value)};
}

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoEnum<T>, absl::StatusOr<Value>>
ProtoEnumToValue(ValueFactory&, T value) {
  if constexpr (std::is_same_v<T, google::protobuf::NullValue>) {
    return NullValue{};
  }
  return IntValue{static_cast<int>(value)};
}

absl::StatusOr<int> ProtoEnumFromValue(
    ValueView value, absl::Nonnull<const google::protobuf::EnumDescriptor*> desc);

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoEnum<T>, absl::StatusOr<T>>
ProtoEnumFromValue(ValueView value) {
  CEL_ASSIGN_OR_RETURN(
      auto enum_value,
      ProtoEnumFromValue(value, google::protobuf::GetEnumDescriptor<T>()));
  return static_cast<T>(enum_value);
}

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::StatusOr<Value>>
ProtoMessageToValue(ValueManager& value_manager, const T& value) {
  if constexpr (std::is_same_v<T, google::protobuf::BoolValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedBoolValueProto(value));
    return BoolValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Int32Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedInt32ValueProto(value));
    return IntValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Int64Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedInt64ValueProto(value));
    return IntValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::UInt32Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedUInt32ValueProto(value));
    return UintValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::UInt64Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedUInt64ValueProto(value));
    return UintValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::FloatValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedFloatValueProto(value));
    return DoubleValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::DoubleValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedDoubleValueProto(value));
    return DoubleValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::BytesValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedBytesValueProto(value));
    return BytesValue{std::move(result)};
  } else if constexpr (std::is_same_v<T, google::protobuf::StringValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedStringValueProto(value));
    return StringValue{std::move(result)};
  } else if constexpr (std::is_same_v<T, google::protobuf::Duration>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedDurationProto(value));
    return DurationValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Timestamp>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedTimestampProto(value));
    return TimestampValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Value>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::GeneratedValueProtoToJson(value));
    return value_manager.CreateValueFromJson(std::move(result));
  } else if constexpr (std::is_same_v<T, google::protobuf::ListValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::GeneratedListValueProtoToJson(value));
    return value_manager.CreateListValueFromJsonArray(std::move(result));
  } else if constexpr (std::is_same_v<T, google::protobuf::Struct>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::GeneratedStructProtoToJson(value));
    return value_manager.CreateMapValueFromJsonObject(std::move(result));
  } else {
    return protobuf_internal::ProtoMessageToValueImpl(
        value_manager, &value, sizeof(T), alignof(T),
        &protobuf_internal::ProtoMessageTraits<T>::ArenaCopyConstruct,
        &protobuf_internal::ProtoMessageTraits<T>::CopyConstruct);
  }
}

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>,
                 absl::StatusOr<ValueView>>
ProtoMessageToValue(ValueManager& value_manager, const T& value,
                    Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  CEL_ASSIGN_OR_RETURN(scratch, ProtoMessageToValue(value_manager, value));
  return scratch;
}

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::StatusOr<Value>>
ProtoMessageToValue(ValueManager& value_manager, T&& value) {
  if constexpr (std::is_same_v<T, google::protobuf::BoolValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedBoolValueProto(value));
    return BoolValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Int32Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedInt32ValueProto(value));
    return IntValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Int64Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedInt64ValueProto(value));
    return IntValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::UInt32Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedUInt32ValueProto(value));
    return UintValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::UInt64Value>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedUInt64ValueProto(value));
    return UintValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::FloatValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedFloatValueProto(value));
    return DoubleValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::DoubleValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedDoubleValueProto(value));
    return DoubleValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::BytesValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedBytesValueProto(value));
    return BytesValue{std::move(result)};
  } else if constexpr (std::is_same_v<T, google::protobuf::StringValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedStringValueProto(value));
    return StringValue{std::move(result)};
  } else if constexpr (std::is_same_v<T, google::protobuf::Duration>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedDurationProto(value));
    return DurationValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Timestamp>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::UnwrapGeneratedTimestampProto(value));
    return TimestampValue{result};
  } else if constexpr (std::is_same_v<T, google::protobuf::Value>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::GeneratedValueProtoToJson(value));
    return value_manager.CreateValueFromJson(std::move(result));
  } else if constexpr (std::is_same_v<T, google::protobuf::ListValue>) {
    CEL_ASSIGN_OR_RETURN(
        auto result, protobuf_internal::GeneratedListValueProtoToJson(value));
    return value_manager.CreateListValueFromJsonArray(std::move(result));
  } else if constexpr (std::is_same_v<T, google::protobuf::Struct>) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         protobuf_internal::GeneratedStructProtoToJson(value));
    return value_manager.CreateMapValueFromJsonObject(std::move(result));
  } else {
    return protobuf_internal::ProtoMessageToValueImpl(
        value_manager, &value, sizeof(T), alignof(T),
        &protobuf_internal::ProtoMessageTraits<T>::ArenaMoveConstruct,
        &protobuf_internal::ProtoMessageTraits<T>::MoveConstruct);
  }
}

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>,
                 absl::StatusOr<ValueView>>
ProtoMessageToValue(ValueManager& value_manager, T&& value,
                    Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  CEL_ASSIGN_OR_RETURN(
      scratch, ProtoMessageToValue(value_manager, std::forward<T>(value)));
  return scratch;
}

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::Status>
ProtoMessageFromValue(ValueView value, T& message,
                      absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
                      absl::Nonnull<google::protobuf::MessageFactory*> factory) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, pool, factory,
                                                      &message);
}

template <typename T>
std::enable_if_t<protobuf_internal::IsProtoMessage<T>, absl::Status>
ProtoMessageFromValue(ValueView value, T& message) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, &message);
}

inline absl::StatusOr<absl::Nonnull<google::protobuf::Message*>> ProtoMessageFromValue(
    ValueView value, absl::Nullable<google::protobuf::Arena*> arena) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, arena);
}

inline absl::StatusOr<absl::Nonnull<google::protobuf::Message*>> ProtoMessageFromValue(
    ValueView value, absl::Nullable<google::protobuf::Arena*> arena,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory) {
  return protobuf_internal::ProtoMessageFromValueImpl(value, pool, factory,
                                                      arena);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_
