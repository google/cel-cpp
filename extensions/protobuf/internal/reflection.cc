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

#include "extensions/protobuf/internal/reflection.h"

#include <string>
#include <utility>

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/nullability.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

bool IsCordField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  return !field->is_extension() &&
         field->options().ctype() == google::protobuf::FieldOptions::CORD;
}

StringValueView GetStringField(
    ValueFactory&, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, StringValue& scratch) {
  if (IsCordField(field)) {
    scratch = StringValue(reflection->GetCord(message, field));
    return scratch;
  }
  std::string value;
  const auto& value_ref =
      reflection->GetStringReference(message, field, &value);
  if (&value_ref == &value) {
    scratch = StringValue{std::move(value)};
    return scratch;
  }
  return StringValueView{value_ref};
}

BytesValueView GetBytesField(
    ValueFactory&, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, BytesValue& scratch) {
  if (IsCordField(field)) {
    scratch = BytesValue(reflection->GetCord(message, field));
    return scratch;
  }
  std::string value;
  const auto& value_ref =
      reflection->GetStringReference(message, field, &value);
  if (&value_ref == &value) {
    scratch = BytesValue{std::move(value)};
    return scratch;
  }
  return BytesValueView{value_ref};
}

}  // namespace cel::extensions::protobuf_internal
