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

#include "common/values/parsed_repeated_field_value.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/value.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

using ::cel::well_known_types::ValueReflection;

std::string ParsedRepeatedFieldValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return "INVALID";
  }
  return "VALID";
}

absl::Status ParsedRepeatedFieldValue::SerializeTo(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<absl::Cord*> value) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(value != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    value->Clear();
    return absl::OkStatus();
  }
  // We have to convert to google.protobuf.Struct first.
  google::protobuf::Value message;
  CEL_RETURN_IF_ERROR(internal::MessageFieldToJson(
      *message_, field_, descriptor_pool, message_factory, &message));
  if (!message.list_value().SerializePartialToCord(value)) {
    return absl::UnknownError("failed to serialize google.protobuf.Struct");
  }
  return absl::OkStatus();
}

absl::Status ParsedRepeatedFieldValue::ConvertToJson(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    ValueReflection value_reflection;
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
    value_reflection.MutableListValue(json)->Clear();
    return absl::OkStatus();
  }
  return internal::MessageFieldToJson(*message_, field_, descriptor_pool,
                                      message_factory, json);
}

absl::Status ParsedRepeatedFieldValue::ConvertToJsonArray(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);
  ABSL_DCHECK(*this);

  json->Clear();

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return absl::OkStatus();
  }
  return internal::MessageFieldToJson(*message_, field_, descriptor_pool,
                                      message_factory, json);
}

absl::Status ParsedRepeatedFieldValue::Equal(
    const Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  if (auto other_value = other.AsParsedRepeatedField(); other_value) {
    ABSL_DCHECK(field_ != nullptr);
    ABSL_DCHECK(other_value->field_ != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal, internal::MessageFieldEquals(
                        *message_, field_, *other_value->message_,
                        other_value->field_, descriptor_pool, message_factory));
    *result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsParsedJsonList(); other_value) {
    if (other_value->value_ == nullptr) {
      *result = BoolValue(IsEmpty());
      return absl::OkStatus();
    }
    ABSL_DCHECK(field_ != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal,
        internal::MessageFieldEquals(*message_, field_, *other_value->value_,
                                     descriptor_pool, message_factory));
    *result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsList(); other_value) {
    return common_internal::ListValueEqual(ListValue(*this), *other_value,
                                           descriptor_pool, message_factory,
                                           arena, result);
  }
  *result = BoolValue(false);
  return absl::OkStatus();
}

bool ParsedRepeatedFieldValue::IsZeroValue() const { return IsEmpty(); }

ParsedRepeatedFieldValue ParsedRepeatedFieldValue::Clone(
    absl::Nonnull<google::protobuf::Arena*> arena) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return ParsedRepeatedFieldValue();
  }
  if (message_.arena() == arena) {
    return *this;
  }
  auto field = message_->GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(
      *message_, field_);
  auto cloned = WrapShared(message_->New(arena), arena);
  auto cloned_field =
      cloned->GetReflection()->GetMutableRepeatedFieldRef<google::protobuf::Message>(
          cel::to_address(cloned), field_);
  cloned_field.CopyFrom(field);
  return ParsedRepeatedFieldValue(std::move(cloned), field_);
}

bool ParsedRepeatedFieldValue::IsEmpty() const { return Size() == 0; }

size_t ParsedRepeatedFieldValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return 0;
  }
  return static_cast<size_t>(GetReflection()->FieldSize(*message_, field_));
}

// See ListValueInterface::Get for documentation.
absl::Status ParsedRepeatedFieldValue::Get(
    size_t index, absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr ||
                         index >= std::numeric_limits<int>::max() ||
                         static_cast<int>(index) >=
                             GetReflection()->FieldSize(*message_, field_))) {
    *result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  *result = Value::RepeatedField(message_, field_, static_cast<int>(index),
                                 descriptor_pool, message_factory);
  return absl::OkStatus();
}

absl::Status ParsedRepeatedFieldValue::ForEach(
    ForEachWithIndexCallback callback,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  const int size = reflection->FieldSize(*message_, field_);
  if (size > 0) {
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         common_internal::RepeatedFieldAccessorFor(field_));
    Value scratch;
    for (int i = 0; i < size; ++i) {
      (*accessor)(arena, message_, field_, reflection, i, descriptor_pool,
                  message_factory, scratch);
      CEL_ASSIGN_OR_RETURN(auto ok, callback(static_cast<size_t>(i), scratch));
      if (!ok) {
        break;
      }
    }
  }
  return absl::OkStatus();
}

namespace {

class ParsedRepeatedFieldValueIterator final : public ValueIterator {
 public:
  ParsedRepeatedFieldValueIterator(
      Owned<const google::protobuf::Message> message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<common_internal::RepeatedFieldAccessor> accessor)
      : message_(std::move(message)),
        field_(field),
        reflection_(message_->GetReflection()),
        accessor_(accessor),
        size_(reflection_->FieldSize(*message_, field_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<Value*> result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next called after ValueIterator::HasNext returned "
          "false");
    }
    (*accessor_)(arena, message_, field_, reflection_, index_, descriptor_pool,
                 message_factory, *result);
    ++index_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Message> message_;
  const absl::Nonnull<const google::protobuf::FieldDescriptor*> field_;
  const absl::Nonnull<const google::protobuf::Reflection*> reflection_;
  const absl::Nonnull<common_internal::RepeatedFieldAccessor> accessor_;
  const int size_;
  int index_ = 0;
};

}  // namespace

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedRepeatedFieldValue::NewIterator() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return NewEmptyValueIterator();
  }
  CEL_ASSIGN_OR_RETURN(auto accessor,
                       common_internal::RepeatedFieldAccessorFor(field_));
  return std::make_unique<ParsedRepeatedFieldValueIterator>(message_, field_,
                                                            accessor);
}

absl::Status ParsedRepeatedFieldValue::Contains(
    const Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  const int size = reflection->FieldSize(*message_, field_);
  if (size > 0) {
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         common_internal::RepeatedFieldAccessorFor(field_));
    Value scratch;
    for (int i = 0; i < size; ++i) {
      (*accessor)(arena, message_, field_, reflection, i, descriptor_pool,
                  message_factory, scratch);
      CEL_RETURN_IF_ERROR(scratch.Equal(other, descriptor_pool, message_factory,
                                        arena, result));
      if (result->IsTrue()) {
        return absl::OkStatus();
      }
    }
  }
  *result = FalseValue();
  return absl::OkStatus();
}

absl::Nonnull<const google::protobuf::Reflection*>
ParsedRepeatedFieldValue::GetReflection() const {
  return message_->GetReflection();
}

}  // namespace cel
