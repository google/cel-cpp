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

#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/generated_enum_util.h"

namespace cel::extensions {

template <typename T>
inline constexpr bool IsProtoEnum = google::protobuf::is_proto_enum<T>::value;

template <typename T>
std::enable_if_t<IsProtoEnum<T>, absl::StatusOr<ValueView>> ProtoEnumToValue(
    ValueFactory&, T value,
    Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND ABSL_ATTRIBUTE_UNUSED) {
  if constexpr (std::is_same_v<T, google::protobuf::NullValue>) {
    return NullValueView{};
  }
  return IntValueView{static_cast<int>(value)};
}

template <typename T>
std::enable_if_t<IsProtoEnum<T>, absl::StatusOr<Value>> ProtoEnumToValue(
    ValueFactory&, T value) {
  if constexpr (std::is_same_v<T, google::protobuf::NullValue>) {
    return NullValue{};
  }
  return IntValue{static_cast<int>(value)};
}

absl::StatusOr<int> ProtoEnumFromValue(
    ValueView value, absl::Nonnull<const google::protobuf::EnumDescriptor*> desc);

template <typename T>
std::enable_if_t<IsProtoEnum<T>, absl::StatusOr<T>> ProtoEnumFromValue(
    ValueView value) {
  CEL_ASSIGN_OR_RETURN(
      auto enum_value,
      ProtoEnumFromValue(value, google::protobuf::GetEnumDescriptor<T>()));
  return static_cast<T>(enum_value);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_
