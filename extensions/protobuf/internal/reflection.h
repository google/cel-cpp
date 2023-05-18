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
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/owner.h"
#include "base/value_factory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<Handle<StringValue>> GetStringField(
    ValueFactory& value_factory, const google::protobuf::Message& message,
    const google::protobuf::Reflection* reflection, const google::protobuf::FieldDescriptor* field)
    ABSL_ATTRIBUTE_NONNULL();

absl::StatusOr<Handle<StringValue>> GetBorrowedStringField(
    ValueFactory& value_factory, Owner<Value> owner,
    const google::protobuf::Message& message, const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field) ABSL_ATTRIBUTE_NONNULL();

absl::StatusOr<Handle<BytesValue>> GetBytesField(
    ValueFactory& value_factory, const google::protobuf::Message& message,
    const google::protobuf::Reflection* reflection, const google::protobuf::FieldDescriptor* field)
    ABSL_ATTRIBUTE_NONNULL();

absl::StatusOr<Handle<BytesValue>> GetBorrowedBytesField(
    ValueFactory& value_factory, Owner<Value> owner,
    const google::protobuf::Message& message, const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field) ABSL_ATTRIBUTE_NONNULL();

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_REFLECTION_H_
