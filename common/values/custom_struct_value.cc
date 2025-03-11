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

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

absl::Status CustomStructValueInterface::Equal(
    const Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  if (auto parsed_struct_value = other.AsCustomStruct();
      parsed_struct_value.has_value() &&
      NativeTypeId::Of(*this) == NativeTypeId::Of(*parsed_struct_value)) {
    return EqualImpl(*parsed_struct_value, descriptor_pool, message_factory,
                     arena, result);
  }
  if (auto struct_value = other.AsStruct(); struct_value.has_value()) {
    return common_internal::StructValueEqual(
        *this, *struct_value, descriptor_pool, message_factory, arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

absl::Status CustomStructValueInterface::EqualImpl(
    const CustomStructValue& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  return common_internal::StructValueEqual(*this, other, descriptor_pool,
                                           message_factory, arena, result);
}

CustomStructValue CustomStructValue::Clone(
    absl::Nonnull<google::protobuf::Arena*> arena) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(!interface_)) {
    return CustomStructValue();
  }
  if (interface_.arena() != arena) {
    return interface_->Clone(arena);
  }
  return *this;
}

absl::Status CustomStructValueInterface::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result,
    absl::Nonnull<int*> count) const {
  return absl::UnimplementedError("Qualify not supported.");
}

}  // namespace cel
