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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_VALUE_H_

#include <type_traits>

#include "absl/base/macros.h"
#include "absl/types/optional.h"
#include "base/values/enum_value.h"
#include "extensions/protobuf/enum_type.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/generated_enum_util.h"

namespace cel::extensions {

class ProtoEnumValue final {
 public:
  static bool Is(const EnumValue& value) {
    return value.type()->Is<ProtoEnumType>();
  }
  static bool Is(const Handle<EnumValue>& value) { return Is(*value); }

  // Retrieves the underlying google::protobuf::EnumValueDescriptor, nullptr is returned
  // if the corresponding google::protobuf::EnumValueDescriptor does not exist.
  static const google::protobuf::EnumValueDescriptor* descriptor(const EnumValue& value);
  static const google::protobuf::EnumValueDescriptor* descriptor(
      const Handle<EnumValue>& value) {
    return descriptor(*value);
  }

  // Converts EnumValue into E, an empty optional is returned if the value
  // cannot be represented as E.
  template <typename E>
  static std::enable_if_t<google::protobuf::is_proto_enum<E>::value, absl::optional<E>>
  value(const EnumValue& value) {
    ABSL_ASSERT(Is(value));
    auto maybe = value_impl(*value.type().As<ProtoEnumType>(), value.number(),
                            google::protobuf::GetEnumDescriptor<E>());
    if (!maybe.has_value()) {
      return absl::nullopt;
    }
    return static_cast<E>(*maybe);
  }
  template <typename E>
  static std::enable_if_t<google::protobuf::is_proto_enum<E>::value, absl::optional<E>>
  value(const Handle<EnumValue>& value) {
    return ProtoEnumValue::value<E>(*value);
  }

 private:
  static absl::optional<int> value_impl(const ProtoEnumType& type,
                                        int64_t number,
                                        const google::protobuf::EnumDescriptor* desc);

  ProtoEnumValue() = delete;
  ProtoEnumValue(const ProtoEnumValue&) = delete;
  ProtoEnumValue(ProtoEnumValue&&) = delete;
  ~ProtoEnumValue() = delete;
  ProtoEnumValue& operator=(const ProtoEnumValue&) = delete;
  ProtoEnumValue& operator=(ProtoEnumValue&&) = delete;
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_VALUE_H_
