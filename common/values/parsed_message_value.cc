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

#include "common/values/parsed_message_value.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/value.h"
#include "extensions/protobuf/internal/qualify.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

using ::cel::well_known_types::ValueReflection;

bool ParsedMessageValue::IsZeroValue() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return true;
  }
  const auto* reflection = GetReflection();
  if (!reflection->GetUnknownFields(*value_).empty()) {
    return false;
  }
  std::vector<const google::protobuf::FieldDescriptor*> fields;
  reflection->ListFields(*value_, &fields);
  return fields.empty();
}

std::string ParsedMessageValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return "INVALID";
  }
  return absl::StrCat(*value_);
}

absl::Status ParsedMessageValue::SerializeTo(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Cord& value) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    value.Clear();
    return absl::OkStatus();
  }

  if (!value_->SerializePartialToString(&value)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", value_->GetTypeName()));
  }
  return absl::OkStatus();
}

absl::Status ParsedMessageValue::ConvertToJson(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);
  ABSL_DCHECK(*this);

  ValueReflection value_reflection;
  CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
  google::protobuf::Message* json_object = value_reflection.MutableStructValue(json);

  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    json_object->Clear();
    return absl::OkStatus();
  }
  return internal::MessageToJson(*value_, descriptor_pool, message_factory,
                                 json_object);
}

absl::Status ParsedMessageValue::ConvertToJsonObject(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    json->Clear();
    return absl::OkStatus();
  }
  return internal::MessageToJson(*value_, descriptor_pool, message_factory,
                                 json);
}

absl::Status ParsedMessageValue::Equal(
    const Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  ABSL_DCHECK(*this);
  if (auto other_message = other.AsParsedMessage(); other_message) {
    CEL_ASSIGN_OR_RETURN(
        auto equal, internal::MessageEquals(*value_, **other_message,
                                            descriptor_pool, message_factory));
    *result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_struct = other.AsStruct(); other_struct) {
    return common_internal::StructValueEqual(StructValue(*this), *other_struct,
                                             descriptor_pool, message_factory,
                                             arena, result);
  }
  *result = BoolValue(false);
  return absl::OkStatus();
}

ParsedMessageValue ParsedMessageValue::Clone(Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return ParsedMessageValue();
  }
  if (value_.arena() == allocator.arena()) {
    return *this;
  }
  auto cloned = WrapShared(value_->New(allocator.arena()), allocator);
  cloned->CopyFrom(*value_);
  return ParsedMessageValue(std::move(cloned));
}

absl::Status ParsedMessageValue::GetFieldByName(
    absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  const auto* descriptor = GetDescriptor();
  const auto* field = descriptor->FindFieldByName(name);
  if (field == nullptr) {
    field = descriptor->file()->pool()->FindExtensionByPrintableName(descriptor,
                                                                     name);
    if (field == nullptr) {
      *result = NoSuchFieldError(name);
      return absl::OkStatus();
    }
  }
  return GetField(field, unboxing_options, descriptor_pool, message_factory,
                  arena, result);
}

absl::Status ParsedMessageValue::GetFieldByNumber(
    int64_t number, ProtoWrapperTypeOptions unboxing_options,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  const auto* descriptor = GetDescriptor();
  if (number < std::numeric_limits<int32_t>::min() ||
      number > std::numeric_limits<int32_t>::max()) {
    *result = NoSuchFieldError(absl::StrCat(number));
    return absl::OkStatus();
  }
  const auto* field = descriptor->FindFieldByNumber(static_cast<int>(number));
  if (field == nullptr) {
    *result = NoSuchFieldError(absl::StrCat(number));
    return absl::OkStatus();
  }
  return GetField(field, unboxing_options, descriptor_pool, message_factory,
                  arena, result);
}

absl::StatusOr<bool> ParsedMessageValue::HasFieldByName(
    absl::string_view name) const {
  const auto* descriptor = GetDescriptor();
  const auto* field = descriptor->FindFieldByName(name);
  if (field == nullptr) {
    field = descriptor->file()->pool()->FindExtensionByPrintableName(descriptor,
                                                                     name);
    if (field == nullptr) {
      return NoSuchFieldError(name).NativeValue();
    }
  }
  return HasField(field);
}

absl::StatusOr<bool> ParsedMessageValue::HasFieldByNumber(
    int64_t number) const {
  const auto* descriptor = GetDescriptor();
  if (number < std::numeric_limits<int32_t>::min() ||
      number > std::numeric_limits<int32_t>::max()) {
    return NoSuchFieldError(absl::StrCat(number)).NativeValue();
  }
  const auto* field = descriptor->FindFieldByNumber(static_cast<int>(number));
  if (field == nullptr) {
    return NoSuchFieldError(absl::StrCat(number)).NativeValue();
  }
  return HasField(field);
}

absl::Status ParsedMessageValue::ForEachField(
    ForEachFieldCallback callback,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return absl::OkStatus();
  }
  std::vector<const google::protobuf::FieldDescriptor*> fields;
  const auto* reflection = GetReflection();
  reflection->ListFields(*value_, &fields);
  for (const auto* field : fields) {
    auto value = Value::Field(value_, field);
    CEL_ASSIGN_OR_RETURN(auto ok, callback(field->name(), value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

namespace {

class ParsedMessageValueQualifyState final
    : public extensions::protobuf_internal::ProtoQualifyState {
 public:
  explicit ParsedMessageValueQualifyState(
      Borrowed<const google::protobuf::Message> message)
      : ProtoQualifyState(cel::to_address(message), message->GetDescriptor(),
                          message->GetReflection()),
        borrower_(message) {}

  absl::optional<Value>& result() { return result_; }

 private:
  void SetResultFromError(absl::Status status, cel::MemoryManagerRef) override {
    result_ = ErrorValue(std::move(status));
  }

  void SetResultFromBool(bool value) override { result_ = BoolValue(value); }

  absl::Status SetResultFromField(const google::protobuf::Message* message,
                                  const google::protobuf::FieldDescriptor* field,
                                  ProtoWrapperTypeOptions unboxing_option,
                                  cel::MemoryManagerRef) override {
    result_ =
        Value::Field(Borrowed(borrower_, message), field, unboxing_option);
    return absl::OkStatus();
  }

  absl::Status SetResultFromRepeatedField(const google::protobuf::Message* message,
                                          const google::protobuf::FieldDescriptor* field,
                                          int index,
                                          cel::MemoryManagerRef) override {
    result_ = Value::RepeatedField(Borrowed(borrower_, message), field, index);
    return absl::OkStatus();
  }

  absl::Status SetResultFromMapField(const google::protobuf::Message* message,
                                     const google::protobuf::FieldDescriptor* field,
                                     const google::protobuf::MapValueConstRef& value,
                                     cel::MemoryManagerRef) override {
    result_ = Value::MapFieldValue(Borrowed(borrower_, message), field, value);
    return absl::OkStatus();
  }

  Borrower borrower_;
  absl::optional<Value> result_;
};

}  // namespace

absl::Status ParsedMessageValue::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result,
    absl::Nonnull<int*> count) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(qualifiers.empty())) {
    return absl::InvalidArgumentError("invalid select qualifier path.");
  }
  ParsedMessageValueQualifyState qualify_state(value_);
  for (int i = 0; i < qualifiers.size() - 1; i++) {
    const auto& qualifier = qualifiers[i];
    CEL_RETURN_IF_ERROR(qualify_state.ApplySelectQualifier(
        qualifier, MemoryManagerRef::Pooling(arena)));
    if (qualify_state.result().has_value()) {
      *result = std::move(qualify_state.result()).value();
      *count = result->Is<ErrorValue>() ? -1 : i + 1;
      return absl::OkStatus();
    }
  }
  const auto& last_qualifier = qualifiers.back();
  if (presence_test) {
    CEL_RETURN_IF_ERROR(qualify_state.ApplyLastQualifierHas(
        last_qualifier, MemoryManagerRef::Pooling(arena)));
  } else {
    CEL_RETURN_IF_ERROR(qualify_state.ApplyLastQualifierGet(
        last_qualifier, MemoryManagerRef::Pooling(arena)));
  }
  *result = std::move(qualify_state.result()).value();
  *count = -1;
  return absl::OkStatus();
}

absl::Status ParsedMessageValue::GetField(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ProtoWrapperTypeOptions unboxing_options,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  *result =
      Value::Field(Borrowed(Borrower::Arena(arena), cel::to_address(value_)),
                   field, descriptor_pool, message_factory, unboxing_options);
  return absl::OkStatus();
}

bool ParsedMessageValue::HasField(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) const {
  const auto* reflection = GetReflection();
  if (field->is_map() || field->is_repeated()) {
    return reflection->FieldSize(*value_, field) > 0;
  }
  return reflection->HasField(*value_, field);
}

}  // namespace cel
