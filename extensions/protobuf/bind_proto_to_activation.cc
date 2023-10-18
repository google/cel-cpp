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

#include "extensions/protobuf/bind_proto_to_activation.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/handle.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "extensions/protobuf/value.h"
#include "internal/status_macros.h"
#include "runtime/activation.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

namespace {

absl::StatusOr<bool> ShouldBindField(
    const google::protobuf::FieldDescriptor* field_desc, const StructValue& struct_value,
    BindProtoUnsetFieldBehavior unset_field_behavior,
    ValueFactory& value_factory) {
  if (unset_field_behavior == BindProtoUnsetFieldBehavior::kBindDefaultValue ||
      field_desc->is_repeated()) {
    return true;
  }
  return struct_value.HasFieldByNumber(value_factory.type_manager(),
                                       field_desc->number());
}

}  // namespace

absl::Status BindProtoToActivation(
    const google::protobuf::Message& context, ValueFactory& value_factory,
    Activation& activation, BindProtoUnsetFieldBehavior unset_field_behavior) {
  CEL_ASSIGN_OR_RETURN(Handle<Value> parent,
                       ProtoValue::Create(value_factory, context));

  if (!parent->Is<StructValue>()) {
    return absl::InvalidArgumentError(
        absl::StrCat("context is a well-known type: ", context.GetTypeName()));
  }

  const google::protobuf::Descriptor* desc = context.GetDescriptor();

  if (desc == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("context missing descriptor: ", context.GetTypeName()));
  }
  const StructValue& struct_value = parent->As<StructValue>();
  for (int i = 0; i < desc->field_count(); i++) {
    const google::protobuf::FieldDescriptor* field_desc = desc->field(i);
    CEL_ASSIGN_OR_RETURN(bool should_bind,
                         ShouldBindField(field_desc, struct_value,
                                         unset_field_behavior, value_factory));
    if (!should_bind) {
      continue;
    }

    CEL_ASSIGN_OR_RETURN(
        Handle<Value> field,
        struct_value.GetFieldByNumber(value_factory, field_desc->number()));

    activation.InsertOrAssignValue(field_desc->name(), std::move(field));
  }

  return absl::OkStatus();
}

}  // namespace cel::extensions
