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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_

#include <type_traits>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/status/statusor.h"
#include "base/type_manager.h"
#include "base/types/wrapper_type.h"
#include "extensions/protobuf/enum_type.h"
#include "extensions/protobuf/struct_type.h"
#include "google/protobuf/generated_enum_util.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

// Utility class for creating and interacting with protocol buffer values.
class ProtoType final {
 private:
  template <typename T>
  using DerivedMessage = std::conjunction<
      std::is_base_of<google::protobuf::Message, std::decay_t<T>>,
      std::negation<std::is_same<google::protobuf::Message, std::decay_t<T>>>>;

  template <typename T>
  using DurationMessage =
      std::is_same<google::protobuf::Duration, std::decay_t<T>>;

  template <typename T>
  static constexpr bool DurationMessageV = DurationMessage<T>::value;

  template <typename T>
  using NotDurationMessage = std::negation<DurationMessage<T>>;

  template <typename T>
  using TimestampMessage =
      std::is_same<google::protobuf::Timestamp, std::decay_t<T>>;

  template <typename T>
  static constexpr bool TimestampMessageV = TimestampMessage<T>::value;

  template <typename T>
  using NotTimestampMessage = std::negation<TimestampMessage<T>>;

  template <typename T>
  using DerivedEnum = google::protobuf::is_proto_enum<std::decay_t<T>>;

  template <typename T>
  using NullWrapperEnum =
      std::is_same<google::protobuf::NullValue, std::decay_t<T>>;

  template <typename T>
  static constexpr bool NullWrapperEnumV = NullWrapperEnum<T>::value;

  template <typename T>
  using NotNullWrapperEnum = std::negation<NullWrapperEnum<T>>;

  template <typename T>
  using BoolWrapperMessage =
      std::is_same<google::protobuf::BoolValue, std::decay_t<T>>;

  template <typename T>
  static constexpr bool BoolWrapperMessageV = BoolWrapperMessage<T>::value;

  template <typename T>
  using BytesWrapperMessage =
      std::is_same<google::protobuf::BytesValue, std::decay_t<T>>;

  template <typename T>
  static constexpr bool BytesWrapperMessageV = BytesWrapperMessage<T>::value;

  template <typename T>
  using DoubleWrapperMessage = std::disjunction<
      std::is_same<google::protobuf::FloatValue, std::decay_t<T>>,
      std::is_same<google::protobuf::DoubleValue, std::decay_t<T>>>;

  template <typename T>
  static constexpr bool DoubleWrapperMessageV = DoubleWrapperMessage<T>::value;

  template <typename T>
  using IntWrapperMessage = std::disjunction<
      std::is_same<google::protobuf::Int32Value, std::decay_t<T>>,
      std::is_same<google::protobuf::Int64Value, std::decay_t<T>>>;

  template <typename T>
  static constexpr bool IntWrapperMessageV = IntWrapperMessage<T>::value;

  template <typename T>
  using StringWrapperMessage =
      std::is_same<google::protobuf::StringValue, std::decay_t<T>>;

  template <typename T>
  static constexpr bool StringWrapperMessageV = StringWrapperMessage<T>::value;

  template <typename T>
  using UintWrapperMessage = std::disjunction<
      std::is_same<google::protobuf::UInt32Value, std::decay_t<T>>,
      std::is_same<google::protobuf::UInt64Value, std::decay_t<T>>>;

  template <typename T>
  static constexpr bool UintWrapperMessageV = UintWrapperMessage<T>::value;

  template <typename T>
  using WrapperMessage =
      std::disjunction<BoolWrapperMessage<T>, BytesWrapperMessage<T>,
                       DoubleWrapperMessage<T>, IntWrapperMessage<T>,
                       StringWrapperMessage<T>, UintWrapperMessage<T>>;

  template <typename T>
  using NotWrapperMessage = std::negation<WrapperMessage<T>>;

  template <typename T>
  using AnyMessage = std::is_same<google::protobuf::Any, std::decay_t<T>>;

  template <typename T>
  using NotAnyMessage = std::negation<AnyMessage<T>>;

 public:
  // Resolve Type from a protocol buffer enum descriptor.
  static absl::StatusOr<Handle<Type>> Resolve(
      TypeManager& type_manager, const google::protobuf::EnumDescriptor& descriptor);

  // Resolve ProtoEnumType from a generated protocol buffer enum.
  template <typename T>
  static std::enable_if_t<
      std::conjunction_v<DerivedEnum<T>, NotNullWrapperEnum<T>>,
      absl::StatusOr<Handle<ProtoEnumType>>>
  Resolve(TypeManager& type_manager) {
    return ProtoEnumType::Resolve<T>(type_manager);
  }

  // Resolve ProtoEnumType from a generated protocol buffer enum.
  template <typename T>
  static std::enable_if_t<NullWrapperEnumV<T>, absl::StatusOr<Handle<NullType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetNullType();
  }

  // Resolve Type from a protocol buffer message descriptor.
  static absl::StatusOr<Handle<Type>> Resolve(
      TypeManager& type_manager, const google::protobuf::Descriptor& descriptor);

  // Resolve ProtoStructType from a generated protocol buffer message.
  template <typename T>
  static std::enable_if_t<
      std::conjunction_v<DerivedMessage<T>, NotDurationMessage<T>,
                         NotTimestampMessage<T>, NotWrapperMessage<T>,
                         NotAnyMessage<T>>,
      absl::StatusOr<Handle<ProtoStructType>>>
  Resolve(TypeManager& type_manager) {
    return ProtoStructType::Resolve<T>(type_manager);
  }

  // Resolve DurationType.
  template <typename T>
  static std::enable_if_t<DurationMessageV<T>,
                          absl::StatusOr<Handle<DurationType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetDurationType();
  }

  // Resolve TimestampType.
  template <typename T>
  static std::enable_if_t<TimestampMessageV<T>,
                          absl::StatusOr<Handle<TimestampType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetTimestampType();
  }

  // Resolve BoolWrapperType.
  template <typename T>
  static std::enable_if_t<BoolWrapperMessageV<T>,
                          absl::StatusOr<Handle<BoolWrapperType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetBoolWrapperType();
  }

  // Resolve BytesWrapperType.
  template <typename T>
  static std::enable_if_t<BytesWrapperMessageV<T>,
                          absl::StatusOr<Handle<BytesWrapperType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetBytesWrapperType();
  }

  // Resolve DoubleWrapperType.
  template <typename T>
  static std::enable_if_t<DoubleWrapperMessageV<T>,
                          absl::StatusOr<Handle<DoubleWrapperType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetDoubleWrapperType();
  }

  // Resolve IntWrapperType.
  template <typename T>
  static std::enable_if_t<IntWrapperMessageV<T>,
                          absl::StatusOr<Handle<IntWrapperType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetIntWrapperType();
  }

  // Resolve StringWrapperType.
  template <typename T>
  static std::enable_if_t<StringWrapperMessageV<T>,
                          absl::StatusOr<Handle<StringWrapperType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetStringWrapperType();
  }

  // Resolve UintWrapperType.
  template <typename T>
  static std::enable_if_t<UintWrapperMessageV<T>,
                          absl::StatusOr<Handle<UintWrapperType>>>
  Resolve(TypeManager& type_manager) {
    return type_manager.type_factory().GetUintWrapperType();
  }

 private:
  ProtoType() = delete;
  ProtoType(const ProtoType&) = delete;
  ProtoType(ProtoType&&) = delete;
  ~ProtoType() = delete;
  ProtoType& operator=(const ProtoType&) = delete;
  ProtoType& operator=(ProtoType&&) = delete;
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_
