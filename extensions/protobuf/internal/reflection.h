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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_REFLECTION_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_REFLECTION_H_

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

bool IsCordField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field);

StringValueView GetStringField(
    ValueFactory& value_factory,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    StringValue& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline StringValue GetStringField(
    ValueFactory& value_factory,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  StringValue scratch;
  auto result =
      GetStringField(value_factory, message, reflection, field, scratch);
  return StringValue{result};
}

BytesValueView GetBytesField(
    ValueFactory& value_factory,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    BytesValue& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline BytesValue GetBytesField(
    ValueFactory& value_factory,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  BytesValue scratch;
  auto result =
      GetBytesField(value_factory, message, reflection, field, scratch);
  return BytesValue{result};
}

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_REFLECTION_H_
