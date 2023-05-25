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

namespace cel::extensions::protobuf_internal {

bool IsCordField(const google::protobuf::FieldDescriptor& field) {
  return !field.is_extension() &&
         field.options().ctype() == google::protobuf::FieldOptions::CORD;
}

absl::StatusOr<Handle<StringValue>> GetStringField(
    ValueFactory& value_factory, const google::protobuf::Message& message,
    const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field) {
  if (IsCordField(*field)) {
    return value_factory.CreateUncheckedStringValue(
        reflection->GetCord(message, field));
  }
  return value_factory.CreateUncheckedStringValue(
      reflection->GetString(message, field));
}

absl::StatusOr<Handle<StringValue>> GetBorrowedStringField(
    ValueFactory& value_factory, Owner<Value> owner,
    const google::protobuf::Message& message, const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field) {
  if (IsCordField(*field)) {
    return value_factory.CreateUncheckedStringValue(
        reflection->GetCord(message, field));
  }
  std::string scratch;
  const std::string& reference =
      reflection->GetStringReference(message, field, &scratch);
  if (&reference == &scratch) {
    return value_factory.CreateUncheckedStringValue(std::move(scratch));
  }
  return value_factory.CreateBorrowedStringValue(std::move(owner),
                                                 absl::string_view(reference));
}

absl::StatusOr<Handle<BytesValue>> GetBytesField(
    ValueFactory& value_factory, const google::protobuf::Message& message,
    const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field) {
  if (IsCordField(*field)) {
    return value_factory.CreateBytesValue(reflection->GetCord(message, field));
  }
  return value_factory.CreateBytesValue(reflection->GetString(message, field));
}

absl::StatusOr<Handle<BytesValue>> GetBorrowedBytesField(
    ValueFactory& value_factory, Owner<Value> owner,
    const google::protobuf::Message& message, const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field) {
  if (IsCordField(*field)) {
    return value_factory.CreateBytesValue(reflection->GetCord(message, field));
  }
  std::string scratch;
  const std::string& reference =
      reflection->GetStringReference(message, field, &scratch);
  if (&reference == &scratch) {
    return value_factory.CreateBytesValue(std::move(scratch));
  }
  return value_factory.CreateBorrowedBytesValue(std::move(owner),
                                                absl::string_view(reference));
}

}  // namespace cel::extensions::protobuf_internal
