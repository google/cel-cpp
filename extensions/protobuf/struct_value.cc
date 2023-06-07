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

// TODO(uncreated-issue/30): get test coverage closer to 100% before using

#include "extensions/protobuf/struct_value.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/container/btree_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/string_value.h"
#include "base/values/uint_value.h"
#include "eval/internal/errors.h"
#include "eval/internal/interop.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "extensions/protobuf/enum_type.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "extensions/protobuf/internal/reflection.h"
#include "extensions/protobuf/internal/time.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/struct_type.h"
#include "extensions/protobuf/type.h"
#include "extensions/protobuf/value.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace cel::interop_internal {

absl::optional<google::api::expr::runtime::MessageWrapper>
ProtoStructValueToMessageWrapper(const Value& value) {
  if (value.Is<extensions::protobuf_internal::ParsedProtoStructValue>()) {
    // "Modern".

    // It's always full protobuf here.
    uintptr_t message =
        reinterpret_cast<uintptr_t>(
            &value.As<extensions::protobuf_internal::ParsedProtoStructValue>()
                 .value()) |
        ::cel::base_internal::kMessageWrapperTagMask;
    uintptr_t type_info = reinterpret_cast<uintptr_t>(
        &::google::api::expr::runtime::GetGenericProtoTypeInfoInstance());
    return MessageWrapperAccess::Make(message, type_info);
  }
  return absl::nullopt;
}

}  // namespace cel::interop_internal

namespace cel::extensions {

namespace protobuf_internal {

namespace {

class HeapDynamicParsedProtoStructValue : public DynamicParsedProtoStructValue {
 public:
  HeapDynamicParsedProtoStructValue(Handle<StructType> type,
                                    const google::protobuf::Message* value)
      : DynamicParsedProtoStructValue(std::move(type), value) {
    ABSL_ASSERT(value->GetArena() == nullptr);
  }

  ~HeapDynamicParsedProtoStructValue() override { delete value_ptr(); }
};

class DynamicMemberParsedProtoStructValue : public ParsedProtoStructValue {
 public:
  DynamicMemberParsedProtoStructValue(Handle<StructType> type,
                                      const google::protobuf::Message* value)
      : ParsedProtoStructValue(std::move(type)),
        value_(ABSL_DIE_IF_NULL(value)) {}  // Crash OK

  const google::protobuf::Message& value() const final { return *value_; }

  absl::optional<const google::protobuf::Message*> ValueReference(
      google::protobuf::Message& scratch, const google::protobuf::Descriptor& desc,
      internal::TypeInfo type) const final {
    if (ABSL_PREDICT_FALSE(&desc != scratch.GetDescriptor())) {
      return absl::nullopt;
    }
    return &value();
  }

 private:
  const google::protobuf::Message* const value_;
};

}  // namespace

}  // namespace protobuf_internal

std::unique_ptr<google::protobuf::Message> ProtoStructValue::value(
    google::protobuf::MessageFactory& message_factory) const {
  return absl::WrapUnique(ValuePointer(message_factory, nullptr));
}

std::unique_ptr<google::protobuf::Message> ProtoStructValue::value() const {
  return absl::WrapUnique(ValuePointer(*type()->factory_, nullptr));
}

google::protobuf::Message* ProtoStructValue::value(
    google::protobuf::Arena& arena, google::protobuf::MessageFactory& message_factory) const {
  return ValuePointer(message_factory, &arena);
}

google::protobuf::Message* ProtoStructValue::value(google::protobuf::Arena& arena) const {
  return ValuePointer(*type()->factory_, &arena);
}

namespace {

std::string DurationValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto duration_or_status =
      protobuf_internal::AbslDurationFromDurationProto(message);
  if (ABSL_PREDICT_FALSE(!duration_or_status.ok())) {
    return std::string("**duration**");
  }
  return DurationValue::DebugString(*duration_or_status);
}

std::string TimestampValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto time_or_status = protobuf_internal::AbslTimeFromTimestampProto(message);
  if (ABSL_PREDICT_FALSE(!time_or_status.ok())) {
    return std::string("**timestamp**");
  }
  return TimestampValue::DebugString(*time_or_status);
}

std::string BoolValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto value_or_status = protobuf_internal::UnwrapBoolValueProto(message);
  if (ABSL_PREDICT_FALSE(!value_or_status.ok())) {
    return std::string("**google.protobuf.BoolValue**");
  }
  return BoolValue::DebugString(*value_or_status);
}

std::string BytesValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto value_or_status = protobuf_internal::UnwrapBytesValueProto(message);
  if (ABSL_PREDICT_FALSE(!value_or_status.ok())) {
    return std::string("**google.protobuf.BytesValue**");
  }
  return BytesValue::DebugString(*value_or_status);
}

std::string DoubleValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto value_or_status = protobuf_internal::UnwrapDoubleValueProto(message);
  if (ABSL_PREDICT_FALSE(!value_or_status.ok())) {
    return std::string("**google.protobuf.DoubleValue**");
  }
  return DoubleValue::DebugString(*value_or_status);
}

std::string IntValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto value_or_status = protobuf_internal::UnwrapIntValueProto(message);
  if (ABSL_PREDICT_FALSE(!value_or_status.ok())) {
    return std::string("**google.protobuf.Int64Value**");
  }
  return IntValue::DebugString(*value_or_status);
}

std::string StringValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto value_or_status = protobuf_internal::UnwrapStringValueProto(message);
  if (ABSL_PREDICT_FALSE(!value_or_status.ok())) {
    return std::string("**google.protobuf.StringValue**");
  }
  return StringValue::DebugString(*value_or_status);
}

std::string UintValueDebugStringFromProto(const google::protobuf::Message& message) {
  auto value_or_status = protobuf_internal::UnwrapUIntValueProto(message);
  if (ABSL_PREDICT_FALSE(!value_or_status.ok())) {
    return std::string("**google.protobuf.UInt64Value**");
  }
  return UintValue::DebugString(*value_or_status);
}

void ProtoDebugStringStruct(std::string& out, const google::protobuf::Message& value) {
  const auto* desc = value.GetDescriptor();
  const auto& full_name = desc->full_name();
  if (full_name == "google.protobuf.Duration") {
    out.append(DurationValueDebugStringFromProto(value));
    return;
  }
  if (full_name == "google.protobuf.Timestamp") {
    out.append(TimestampValueDebugStringFromProto(value));
    return;
  }
  if (full_name == "google.protobuf.BoolValue") {
    out.append(BoolValueDebugStringFromProto(value));
    return;
  }
  if (full_name == "google.protobuf.BytesValue") {
    out.append(BytesValueDebugStringFromProto(value));
    return;
  }
  if (full_name == "google.protobuf.DoubleValue" ||
      full_name == "google.protobuf.FloatValue") {
    out.append(DoubleValueDebugStringFromProto(value));
    return;
  }
  if (full_name == "google.protobuf.Int32Value" ||
      full_name == "google.protobuf.Int64Value") {
    out.append(IntValueDebugStringFromProto(value));
    return;
  }
  if (full_name == "google.protobuf.StringValue") {
    out.append(StringValueDebugStringFromProto(value));
    return;
  }
  if (full_name == "google.protobuf.UInt32Value" ||
      full_name == "google.protobuf.UInt64Value") {
    out.append(UintValueDebugStringFromProto(value));
    return;
  }
  out.append(protobuf_internal::ParsedProtoStructValue::DebugString(value));
}

template <typename T, typename P = void>
class ParsedProtoListValue;
template <typename T, typename P = void>
class ArenaParsedProtoListValue;
template <typename T, typename P = void>
class ReffedParsedProtoListValue;

template <>
class ParsedProtoListValue<NullValue> : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type, size_t size)
      : CEL_LIST_VALUE_CLASS(std::move(type)), size_(size) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    size_t field = 0;
    if (field != size_) {
      out.append(NullValue::DebugString());
      ++field;
      for (; field != size_; ++field) {
        out.append(", ");
        out.append(NullValue::DebugString());
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return size_; }

  bool empty() const final { return size_ == 0; }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    ABSL_ASSERT(index < size_);
    return context.value_factory().GetNullValue();
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<NullValue>>();
  }

  const size_t size_;
};

template <>
class ParsedProtoListValue<BoolValue, bool> : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<bool> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(BoolValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(BoolValue::DebugString(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return context.value_factory().CreateBoolValue(
        fields_.Get(static_cast<int>(index)));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<BoolValue, bool>>();
  }

  const google::protobuf::RepeatedFieldRef<bool> fields_;
};

template <typename P>
class ParsedProtoListValue<IntValue, P> : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<P> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(IntValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(IntValue::DebugString(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return context.value_factory().CreateIntValue(
        fields_.Get(static_cast<int>(index)));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<IntValue, P>>();
  }

  const google::protobuf::RepeatedFieldRef<P> fields_;
};

template <typename P>
class ParsedProtoListValue<UintValue, P> : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<P> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(UintValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(UintValue::DebugString(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return context.value_factory().CreateUintValue(
        fields_.Get(static_cast<int>(index)));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<UintValue, P>>();
  }

  const google::protobuf::RepeatedFieldRef<P> fields_;
};

template <typename P>
class ParsedProtoListValue<DoubleValue, P> : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<P> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(DoubleValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(DoubleValue::DebugString(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return context.value_factory().CreateDoubleValue(
        fields_.Get(static_cast<int>(index)));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<DoubleValue, P>>();
  }

  const google::protobuf::RepeatedFieldRef<P> fields_;
};

template <>
class ParsedProtoListValue<BytesValue, std::string>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<std::string> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(BytesValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(BytesValue::DebugString(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    // Proto does not provide a zero copy interface for accessing repeated bytes
    // fields.
    return context.value_factory().CreateBytesValue(
        fields_.Get(static_cast<int>(index)));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<BytesValue, std::string>>();
  }

  const google::protobuf::RepeatedFieldRef<std::string> fields_;
};

template <>
class ParsedProtoListValue<StringValue, std::string>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<std::string> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(StringValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(StringValue::DebugString(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    // Proto does not provide a zero copy interface for accessing repeated
    // string fields.
    return context.value_factory().CreateUncheckedStringValue(
        fields_.Get(static_cast<int>(index)));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<StringValue, std::string>>();
  }

  const google::protobuf::RepeatedFieldRef<std::string> fields_;
};

template <>
class ParsedProtoListValue<DurationValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(DurationValueDebugStringFromProto(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(DurationValueDebugStringFromProto(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    CEL_ASSIGN_OR_RETURN(
        auto duration,
        protobuf_internal::AbslDurationFromDurationProto(
            fields_.Get(static_cast<int>(index), scratch.get())));
    scratch.reset();
    return context.value_factory().CreateUncheckedDurationValue(duration);
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<
        ParsedProtoListValue<DurationValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

template <>
class ParsedProtoListValue<TimestampValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(TimestampValueDebugStringFromProto(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(TimestampValueDebugStringFromProto(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    CEL_ASSIGN_OR_RETURN(
        auto time, protobuf_internal::AbslTimeFromTimestampProto(
                       fields_.Get(static_cast<int>(index), scratch.get())));
    scratch.reset();
    return context.value_factory().CreateUncheckedTimestampValue(time);
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<
        ParsedProtoListValue<TimestampValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

template <>
class ParsedProtoListValue<EnumValue, int32_t> : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<int32_t> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(
          EnumValue::DebugString(*type()->element().As<EnumType>(), *field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(
            EnumValue::DebugString(*type()->element().As<EnumType>(), *field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return context.value_factory().CreateEnumValue(
        type()->element().As<EnumType>(), fields_.Get(static_cast<int>(index)));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<EnumValue, int32_t>>();
  }

  const google::protobuf::RepeatedFieldRef<int32_t> fields_;
};

template <>
class ParsedProtoListValue<ProtoStructValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      out.append(
          protobuf_internal::ParsedProtoStructValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(
            protobuf_internal::ParsedProtoStructValue::DebugString(*field));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    if (&field != scratch.get()) {
      // Scratch was not used, we can avoid copying.
      scratch.reset();
      return context.value_factory()
          .CreateBorrowedStructValue<
              protobuf_internal::DynamicMemberParsedProtoStructValue>(
              owner_from_this(), type()->element().As<StructType>(), &field);
    }
    if (ProtoMemoryManager::Is(context.value_factory().memory_manager())) {
      auto* arena = ProtoMemoryManager::CastToProtoArena(
          context.value_factory().memory_manager());
      if (ABSL_PREDICT_TRUE(arena != nullptr)) {
        // We are using google::protobuf::Arena, but fields_.NewMessage() allocates on the
        // heap. Copy the message into the arena to avoid the extra bookkeeping.
        auto* message = field.New(arena);
        message->CopyFrom(*scratch);
        scratch.reset();
        return context.value_factory()
            .CreateStructValue<
                protobuf_internal::ArenaDynamicParsedProtoStructValue>(
                type()->element().As<ProtoStructType>(), message);
      }
    }
    return context.value_factory()
        .CreateStructValue<
            protobuf_internal::HeapDynamicParsedProtoStructValue>(
            type()->element().As<ProtoStructType>(), scratch.release());
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<
        ParsedProtoListValue<ProtoStructValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.ListValue
template <>
class ParsedProtoListValue<ListValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    if (scratch.get() == &field) {
      return protobuf_internal::CreateListValue(context.value_factory(),
                                                std::move(scratch));
    }
    scratch.reset();
    return protobuf_internal::CreateBorrowedListValue(
        owner_from_this(), context.value_factory(), field);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<ListValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.Struct
template <>
class ParsedProtoListValue<MapValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    if (scratch.get() == &field) {
      return protobuf_internal::CreateStruct(context.value_factory(),
                                             std::move(scratch));
    }
    scratch.reset();
    return protobuf_internal::CreateBorrowedStruct(
        owner_from_this(), context.value_factory(), field);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<MapValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.Value
template <>
class ParsedProtoListValue<DynValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    if (scratch.get() == &field) {
      return protobuf_internal::CreateValue(context.value_factory(),
                                            std::move(scratch));
    }
    scratch.reset();
    return protobuf_internal::CreateBorrowedValue(
        owner_from_this(), context.value_factory(), field);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<DynValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.Any
template <>
class ParsedProtoListValue<AnyType, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    return ProtoValue::Create(context.value_factory(), field);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<ListValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.BoolValue
template <>
class ParsedProtoListValue<BoolValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapBoolValueProto(field));
    return context.value_factory().CreateBoolValue(wrapped);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<BoolValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.BytesValue
template <>
class ParsedProtoListValue<BytesValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapBytesValueProto(field));
    return context.value_factory().CreateBytesValue(std::move(wrapped));
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<
        ParsedProtoListValue<BytesValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.{FloatValue,DoubleValue}
template <>
class ParsedProtoListValue<DoubleValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapDoubleValueProto(field));
    return context.value_factory().CreateDoubleValue(wrapped);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<
        ParsedProtoListValue<DoubleValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.{Int32Value,Int64Value}
template <>
class ParsedProtoListValue<IntValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapIntValueProto(field));
    return context.value_factory().CreateIntValue(wrapped);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<IntValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.StringValue
template <>
class ParsedProtoListValue<StringValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapStringValueProto(field));
    return context.value_factory().CreateUncheckedStringValue(
        std::move(wrapped));
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<
        ParsedProtoListValue<StringValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

// repeated google.protobuf.{UInt32Value,UInt64Value}
template <>
class ParsedProtoListValue<UintValue, google::protobuf::Message>
    : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoListValue(Handle<ListType> type,
                       google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields)
      : CEL_LIST_VALUE_CLASS(std::move(type)), fields_(std::move(fields)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto field = fields_.begin();
    if (field != fields_.end()) {
      ProtoDebugStringStruct(out, *field);
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        ProtoDebugStringStruct(out, *field);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return fields_.size(); }

  bool empty() const final { return fields_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    CEL_ASSIGN_OR_RETURN(auto wrapped,
                         protobuf_internal::UnwrapUIntValueProto(field));
    return context.value_factory().CreateUintValue(wrapped);
  }

 private:
  cel::internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoListValue<UintValue, google::protobuf::Message>>();
  }

  const google::protobuf::RepeatedFieldRef<google::protobuf::Message> fields_;
};

void ProtoDebugStringEnum(std::string& out, const google::protobuf::EnumDescriptor& desc,
                          int32_t value) {
  if (desc.full_name() == "google.protobuf.NullValue") {
    out.append(NullValue::DebugString());
    return;
  }
  const auto* value_desc = desc.FindValueByNumber(value);
  if (value_desc != nullptr) {
    absl::StrAppend(&out, desc.full_name(), ".", value_desc->name());
    return;
  }
  absl::StrAppend(&out, desc.full_name(), "(", value, ")");
}

void ProtoDebugStringMapKey(std::string& out, const google::protobuf::MapKey& key) {
  switch (key.type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      out.append(IntValue::DebugString(key.GetInt64Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      out.append(IntValue::DebugString(key.GetInt32Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      out.append(UintValue::DebugString(key.GetUInt64Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      out.append(UintValue::DebugString(key.GetUInt32Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      out.append(StringValue::DebugString(key.GetStringValue()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      out.append(BoolValue::DebugString(key.GetBoolValue()));
      break;
    default:
      // Unreachable because protobuf is extremely unlikely to introduce
      // additional supported key types.
      ABSL_UNREACHABLE();
  }
}

void ProtoDebugStringMapValue(std::string& out,
                              const google::protobuf::FieldDescriptor& field,
                              const google::protobuf::MapValueConstRef& value) {
  switch (field.cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      out.append(IntValue::DebugString(value.GetInt64Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      out.append(IntValue::DebugString(value.GetInt32Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      out.append(UintValue::DebugString(value.GetUInt64Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      out.append(UintValue::DebugString(value.GetUInt32Value()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (field.type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        out.append(BytesValue::DebugString(value.GetStringValue()));
      } else {
        out.append(StringValue::DebugString(value.GetStringValue()));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      out.append(BoolValue::DebugString(value.GetBoolValue()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      out.append(DoubleValue::DebugString(value.GetFloatValue()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      out.append(DoubleValue::DebugString(value.GetDoubleValue()));
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      ProtoDebugStringEnum(out, *field.enum_type(), value.GetEnumValue());
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      ProtoDebugStringStruct(out, value.GetMessageValue());
      break;
  }
}

void ProtoDebugStringMapValue(std::string& out,
                              const google::protobuf::Reflection& reflect,
                              const google::protobuf::Message& message,
                              const google::protobuf::FieldDescriptor& field,
                              const google::protobuf::FieldDescriptor& value_desc,
                              const google::protobuf::MapKey& key) {
  google::protobuf::MapValueConstRef value;
  bool success =
      protobuf_internal::LookupMapValue(reflect, message, field, key, &value);
  ABSL_ASSERT(success);
  ProtoDebugStringMapValue(out, value_desc, value);
}

void ProtoDebugStringMap(std::string& out, const google::protobuf::Message& message,
                         const google::protobuf::Reflection* reflect,
                         const google::protobuf::FieldDescriptor* field_desc) {
  absl::btree_set<google::protobuf::MapKey> sorted_keys;
  {
    auto begin = protobuf_internal::MapBegin(*reflect, message, *field_desc);
    auto end = protobuf_internal::MapEnd(*reflect, message, *field_desc);
    for (; begin != end; ++begin) {
      sorted_keys.insert(begin.GetKey());
    }
  }
  const auto* value_desc = field_desc->message_type()->map_value();
  out.push_back('{');
  auto key = sorted_keys.begin();
  auto key_end = sorted_keys.end();
  if (key != key_end) {
    ProtoDebugStringMapKey(out, *key);
    out.append(": ");
    ProtoDebugStringMapValue(out, *reflect, message, *field_desc, *value_desc,
                             *key);
    ++key;
    for (; key != key_end; ++key) {
      out.append(", ");
      ProtoDebugStringMapKey(out, *key);
      out.append(": ");
      ProtoDebugStringMapValue(out, *reflect, message, *field_desc, *value_desc,
                               *key);
    }
  }
  out.push_back('}');
}

// Transform Value into MapKey. Requires that value is compatible with protocol
// buffer map key.
bool ToProtoMapKey(google::protobuf::MapKey& key, const Handle<Value>& value,
                   const google::protobuf::FieldDescriptor& field) {
  switch (value->kind()) {
    case ValueKind::kBool:
      key.SetBoolValue(value.As<BoolValue>()->value());
      break;
    case ValueKind::kInt: {
      int64_t cpp_key = value.As<IntValue>()->value();
      const auto* key_desc = field.message_type()->map_key();
      switch (key_desc->cpp_type()) {
        case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
          key.SetInt64Value(cpp_key);
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
          if (cpp_key < std::numeric_limits<int32_t>::min() ||
              cpp_key > std::numeric_limits<int32_t>::max()) {
            return false;
          }
          key.SetInt32Value(static_cast<int32_t>(cpp_key));
          break;
        default:
          ABSL_UNREACHABLE();
      }
    } break;
    case ValueKind::kUint: {
      uint64_t cpp_key = value.As<UintValue>()->value();
      const auto* key_desc = field.message_type()->map_key();
      switch (key_desc->cpp_type()) {
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
          key.SetUInt64Value(cpp_key);
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
          if (cpp_key > std::numeric_limits<uint32_t>::max()) {
            return false;
          }
          key.SetUInt32Value(static_cast<uint32_t>(cpp_key));
          break;
        default:
          ABSL_UNREACHABLE();
      }
    } break;
    case ValueKind::kString:
      key.SetStringValue(value.As<StringValue>()->ToString());
      break;
    default:
      // Unreachable because protobuf is extremely unlikely to introduce
      // additional supported key types.
      ABSL_UNREACHABLE();
  }
  return true;
}

class ParsedProtoMapValueKeysList : public CEL_LIST_VALUE_CLASS {
 public:
  ParsedProtoMapValueKeysList(
      Handle<ListType> type,
      std::vector<google::protobuf::MapKey, Allocator<google::protobuf::MapKey>> keys)
      : CEL_LIST_VALUE_CLASS(std::move(type)), keys_(std::move(keys)) {}

  std::string DebugString() const final {
    std::string out;
    out.push_back('[');
    auto element = keys_.begin();
    if (element != keys_.end()) {
      ProtoDebugStringMapKey(out, *element);
      ++element;
      for (; element != keys_.end(); ++element) {
        out.append(", ");
        ProtoDebugStringMapKey(out, *element);
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const final { return keys_.size(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    const auto& key = keys_[index];
    switch (key.type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        return context.value_factory().CreateIntValue(key.GetInt64Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        return context.value_factory().CreateIntValue(key.GetInt32Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        return context.value_factory().CreateUintValue(key.GetUInt64Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        return context.value_factory().CreateUintValue(key.GetUInt32Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        return context.value_factory().CreateBorrowedStringValue(
            owner_from_this(), key.GetStringValue());
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        return context.value_factory().CreateBoolValue(key.GetBoolValue());
      default:
        // Unreachable because protobuf is extremely unlikely to introduce
        // additional supported key types.
        ABSL_UNREACHABLE();
    }
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoMapValueKeysList>();
  }

  const std::vector<google::protobuf::MapKey, Allocator<google::protobuf::MapKey>> keys_;
};

class ParsedProtoMapValue : public CEL_MAP_VALUE_CLASS {
 public:
  ParsedProtoMapValue(Handle<MapType> type, const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor& field)
      : CEL_MAP_VALUE_CLASS(std::move(type)),
        message_(message),
        field_(field) {}

  std::string DebugString() const final {
    std::string out;
    ProtoDebugStringMap(out, message_, &reflection(), &field_);
    return out;
  }

  size_t size() const final {
    return protobuf_internal::MapSize(reflection(), message_, field_);
  }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      const GetContext& context, const Handle<Value>& key) const final {
    if (ABSL_PREDICT_FALSE(type()->key() != key->type())) {
      return absl::InvalidArgumentError(absl::StrCat(
          "map key type mismatch, expected: ", type()->key()->DebugString(),
          " got: ", key->type()->DebugString()));
    }
    google::protobuf::MapKey proto_key;
    if (ABSL_PREDICT_FALSE(!ToProtoMapKey(proto_key, key, field_))) {
      return absl::InvalidArgumentError(
          "unable to convert value to protocol buffer map key");
    }
    google::protobuf::MapValueConstRef proto_value;
    if (!protobuf_internal::LookupMapValue(reflection(), message_, field_,
                                           proto_key, &proto_value)) {
      return absl::nullopt;
    }
    const auto* value_desc = field_.message_type()->map_value();
    switch (value_desc->cpp_type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        return context.value_factory().CreateBoolValue(
            proto_value.GetBoolValue());
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        return context.value_factory().CreateIntValue(
            proto_value.GetInt64Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        return context.value_factory().CreateIntValue(
            proto_value.GetInt32Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        return context.value_factory().CreateUintValue(
            proto_value.GetUInt64Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        return context.value_factory().CreateUintValue(
            proto_value.GetUInt32Value());
      case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        return context.value_factory().CreateDoubleValue(
            proto_value.GetFloatValue());
      case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        return context.value_factory().CreateDoubleValue(
            proto_value.GetDoubleValue());
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
        if (value_desc->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
          return context.value_factory().CreateBorrowedBytesValue(
              owner_from_this(), proto_value.GetStringValue());
        } else {
          return context.value_factory().CreateBorrowedStringValue(
              owner_from_this(), proto_value.GetStringValue());
        }
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
        CEL_ASSIGN_OR_RETURN(
            auto type,
            ProtoType::Resolve(context.value_factory().type_manager(),
                               *value_desc->enum_type()));
        switch (type->kind()) {
          case TypeKind::kNullType:
            return context.value_factory().GetNullValue();
          case TypeKind::kEnum:
            return context.value_factory().CreateEnumValue(
                std::move(type).As<ProtoEnumType>(),
                proto_value.GetEnumValue());
          default:
            return absl::InternalError(absl::StrCat(
                "Unexpected protocol buffer type implementation for \"",
                value_desc->message_type()->full_name(),
                "\": ", type->DebugString()));
        }
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
        CEL_ASSIGN_OR_RETURN(
            auto type,
            ProtoType::Resolve(context.value_factory().type_manager(),
                               *value_desc->message_type()));
        switch (type->kind()) {
          case TypeKind::kDuration: {
            CEL_ASSIGN_OR_RETURN(
                auto duration, protobuf_internal::AbslDurationFromDurationProto(
                                   proto_value.GetMessageValue()));
            return context.value_factory().CreateUncheckedDurationValue(
                duration);
          }
          case TypeKind::kTimestamp: {
            CEL_ASSIGN_OR_RETURN(auto time,
                                 protobuf_internal::AbslTimeFromTimestampProto(
                                     proto_value.GetMessageValue()));
            return context.value_factory().CreateUncheckedTimestampValue(time);
          }
          case TypeKind::kList:
            // google.protobuf.ListValue
            return protobuf_internal::CreateBorrowedListValue(
                owner_from_this(), context.value_factory(),
                proto_value.GetMessageValue());
          case TypeKind::kMap:
            // google.protobuf.Struct
            return protobuf_internal::CreateBorrowedStruct(
                owner_from_this(), context.value_factory(),
                proto_value.GetMessageValue());
          case TypeKind::kDyn:
            // google.protobuf.Value
            return protobuf_internal::CreateBorrowedValue(
                owner_from_this(), context.value_factory(),
                proto_value.GetMessageValue());
          case TypeKind::kAny:
            return ProtoValue::Create(context.value_factory(),
                                      proto_value.GetMessageValue());
          case TypeKind::kWrapper:
            switch (type->As<WrapperType>().wrapped()->kind()) {
              case TypeKind::kBool: {
                // google.protobuf.BoolValue, mapped to CEL primitive bool type
                // for map values.
                CEL_ASSIGN_OR_RETURN(auto wrapped,
                                     protobuf_internal::UnwrapBoolValueProto(
                                         proto_value.GetMessageValue()));
                return context.value_factory().CreateBoolValue(wrapped);
              }
              case TypeKind::kBytes: {
                // google.protobuf.BytesValue, mapped to CEL primitive bytes
                // type for map values.
                CEL_ASSIGN_OR_RETURN(auto wrapped,
                                     protobuf_internal::UnwrapBytesValueProto(
                                         proto_value.GetMessageValue()));
                return context.value_factory().CreateBytesValue(
                    std::move(wrapped));
              }
              case TypeKind::kDouble: {
                // google.protobuf.{FloatValue,DoubleValue}, mapped to CEL
                // primitive double type for map values.
                CEL_ASSIGN_OR_RETURN(auto wrapped,
                                     protobuf_internal::UnwrapDoubleValueProto(
                                         proto_value.GetMessageValue()));
                return context.value_factory().CreateDoubleValue(wrapped);
              }
              case TypeKind::kInt: {
                // google.protobuf.{Int32Value,Int64Value}, mapped to CEL
                // primitive int type for map values.
                CEL_ASSIGN_OR_RETURN(auto wrapped,
                                     protobuf_internal::UnwrapIntValueProto(
                                         proto_value.GetMessageValue()));
                return context.value_factory().CreateIntValue(wrapped);
              }
              case TypeKind::kString: {
                // google.protobuf.StringValue, mapped to CEL primitive bytes
                // type for map values.
                CEL_ASSIGN_OR_RETURN(auto wrapped,
                                     protobuf_internal::UnwrapStringValueProto(
                                         proto_value.GetMessageValue()));
                return context.value_factory().CreateUncheckedStringValue(
                    std::move(wrapped));
              }
              case TypeKind::kUint: {
                // google.protobuf.{UInt32Value,UInt64Value}, mapped to CEL
                // primitive uint type for map values.
                CEL_ASSIGN_OR_RETURN(auto wrapped,
                                     protobuf_internal::UnwrapUIntValueProto(
                                         proto_value.GetMessageValue()));
                return context.value_factory().CreateUintValue(wrapped);
              }
              default:
                ABSL_UNREACHABLE();
            }
          case TypeKind::kStruct:
            return context.value_factory()
                .CreateBorrowedStructValue<
                    protobuf_internal::DynamicMemberParsedProtoStructValue>(
                    owner_from_this(), std::move(type).As<ProtoStructType>(),
                    &proto_value.GetMessageValue());
          default:
            return absl::InternalError(absl::StrCat(
                "Unexpected protocol buffer type implementation for \"",
                value_desc->message_type()->full_name(),
                "\": ", type->DebugString()));
        }
      }
    }
  }

  absl::StatusOr<bool> Has(const HasContext& context,
                           const Handle<Value>& key) const final {
    if (ABSL_PREDICT_FALSE(type()->key() != key->type())) {
      return absl::InvalidArgumentError(absl::StrCat(
          "map key type mismatch, expected: ", type()->key()->DebugString(),
          " got: ", type()->value()->DebugString()));
    }
    google::protobuf::MapKey proto_key;
    if (ABSL_PREDICT_FALSE(!ToProtoMapKey(proto_key, key, field_))) {
      return absl::InvalidArgumentError(
          "unable to convert value to protocol buffer map key");
    }
    return protobuf_internal::ContainsMapKey(reflection(), message_, field_,
                                             proto_key);
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      const ListKeysContext& context) const final {
    CEL_ASSIGN_OR_RETURN(
        auto list_type,
        context.value_factory().type_factory().CreateListType(type()->key()));
    std::vector<google::protobuf::MapKey, Allocator<google::protobuf::MapKey>> keys(
        Allocator<google::protobuf::MapKey>(context.value_factory().memory_manager()));
    keys.reserve(size());
    auto begin = protobuf_internal::MapBegin(reflection(), message_, field_);
    auto end = protobuf_internal::MapEnd(reflection(), message_, field_);
    for (; begin != end; ++begin) {
      keys.push_back(begin.GetKey());
    }
    return context.value_factory()
        .CreateBorrowedListValue<ParsedProtoMapValueKeysList>(
            owner_from_this(), std::move(list_type), std::move(keys));
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoMapValue>();
  }

  const google::protobuf::Reflection& reflection() const {
    return *ABSL_DIE_IF_NULL(message_.GetReflection());  // Crash OK
  }

  const google::protobuf::Message& message_;
  const google::protobuf::FieldDescriptor& field_;
};

void ProtoDebugStringSingular(std::string& out, const google::protobuf::Message& message,
                              const google::protobuf::Reflection* reflect,
                              const google::protobuf::FieldDescriptor* field_desc) {
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      out.append(
          DoubleValue::DebugString(reflect->GetDouble(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      out.append(
          DoubleValue::DebugString(reflect->GetFloat(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      out.append(IntValue::DebugString(reflect->GetInt64(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      out.append(IntValue::DebugString(reflect->GetInt32(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      out.append(
          UintValue::DebugString(reflect->GetUInt64(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      out.append(
          UintValue::DebugString(reflect->GetUInt32(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      out.append(BoolValue::DebugString(reflect->GetBool(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_STRING: {
      std::string scratch;
      out.append(StringValue::DebugString(
          reflect->GetStringReference(message, field_desc, &scratch)));
    } break;
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      ProtoDebugStringStruct(out, reflect->GetMessage(message, field_desc));
      break;
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      std::string scratch;
      out.append(BytesValue::DebugString(
          reflect->GetStringReference(message, field_desc, &scratch)));
    } break;
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      ProtoDebugStringEnum(out, *field_desc->enum_type(),
                           reflect->GetEnumValue(message, field_desc));
      break;
  }
}

void ProtoDebugStringRepeated(std::string& out, const google::protobuf::Message& message,
                              const google::protobuf::Reflection* reflect,
                              const google::protobuf::FieldDescriptor* field_desc) {
  out.push_back('[');
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE: {
      auto fields = reflect->GetRepeatedFieldRef<double>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(DoubleValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(DoubleValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT: {
      auto fields = reflect->GetRepeatedFieldRef<float>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(DoubleValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(DoubleValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64: {
      auto fields = reflect->GetRepeatedFieldRef<int64_t>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(IntValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(IntValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32: {
      auto fields = reflect->GetRepeatedFieldRef<int32_t>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(IntValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(IntValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64: {
      auto fields = reflect->GetRepeatedFieldRef<uint64_t>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(UintValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(UintValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32: {
      auto fields = reflect->GetRepeatedFieldRef<uint32_t>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(UintValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(UintValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_BOOL: {
      auto fields = reflect->GetRepeatedFieldRef<bool>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(BoolValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(BoolValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_STRING: {
      auto fields =
          reflect->GetRepeatedFieldRef<std::string>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(StringValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(StringValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE: {
      auto fields =
          reflect->GetRepeatedFieldRef<google::protobuf::Message>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        ProtoDebugStringStruct(out, *field);
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          ProtoDebugStringStruct(out, *field);
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      auto fields =
          reflect->GetRepeatedFieldRef<std::string>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(BytesValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(BytesValue::DebugString(*field));
        }
      }
    } break;
    case google::protobuf::FieldDescriptor::TYPE_ENUM: {
      auto fields = reflect->GetRepeatedFieldRef<int32_t>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        ProtoDebugStringEnum(out, *field_desc->enum_type(), *field);
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          ProtoDebugStringEnum(out, *field_desc->enum_type(), *field);
        }
      }
    } break;
  }
  out.push_back(']');
}

void ProtoDebugString(std::string& out, const google::protobuf::Message& message,
                      const google::protobuf::Reflection* reflect,
                      const google::protobuf::FieldDescriptor* field_desc) {
  if (field_desc->is_map()) {
    ProtoDebugStringMap(out, message, reflect, field_desc);
    return;
  }
  if (field_desc->is_repeated()) {
    ProtoDebugStringRepeated(out, message, reflect, field_desc);
    return;
  }
  ProtoDebugStringSingular(out, message, reflect, field_desc);
}

}  // namespace

absl::StatusOr<Handle<ProtoStructValue>> ProtoStructValue::Create(
    ValueFactory& value_factory, const google::protobuf::Message& message) {
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError("message missing descriptor");
  }
  CEL_ASSIGN_OR_RETURN(
      auto type,
      ProtoStructType::Resolve(value_factory.type_manager(), *descriptor));
  bool same_descriptors = &type->descriptor() == descriptor;
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (ABSL_PREDICT_TRUE(arena != nullptr)) {
      google::protobuf::Message* value;
      if (ABSL_PREDICT_TRUE(same_descriptors)) {
        value = message.New(arena);
        value->CopyFrom(message);
      } else {
        const auto* prototype =
            type->factory_->GetPrototype(&type->descriptor());
        if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
          return absl::InternalError(absl::StrCat(
              "cel: unable to get prototype for protocol buffer message \"",
              type->name(), "\""));
        }
        value = prototype->New(arena);
        std::string serialized;
        if (ABSL_PREDICT_FALSE(
                !message.SerializePartialToString(&serialized))) {
          return absl::InternalError(
              "cel: failed to serialize protocol buffer message");
        }
        if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
          return absl::InternalError(
              "cel: failed to deserialize protocol buffer message");
        }
      }
      return value_factory.CreateStructValue<
          protobuf_internal::ArenaDynamicParsedProtoStructValue>(type, value);
    }
  }
  std::unique_ptr<google::protobuf::Message> value;
  if (ABSL_PREDICT_TRUE(same_descriptors)) {
    value = absl::WrapUnique(message.New());
    value->CopyFrom(message);
  } else {
    const auto* prototype = type->factory_->GetPrototype(&type->descriptor());
    if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
      return absl::InternalError(absl::StrCat(
          "cel: unable to get prototype for protocol buffer message \"",
          type->name(), "\""));
    }
    value = absl::WrapUnique(prototype->New());
    std::string serialized;
    if (ABSL_PREDICT_FALSE(!message.SerializePartialToString(&serialized))) {
      return absl::InternalError(
          "cel: failed to serialize protocol buffer message");
    }
    if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
      return absl::InternalError(
          "cel: failed to deserialize protocol buffer message");
    }
  }
  auto status_or_message = value_factory.CreateStructValue<
      protobuf_internal::HeapDynamicParsedProtoStructValue>(type, value.get());
  if (ABSL_PREDICT_FALSE(!status_or_message.ok())) {
    return status_or_message.status();
  }
  value.release();
  return std::move(status_or_message).value();
}

absl::StatusOr<Handle<ProtoStructValue>> ProtoStructValue::CreateBorrowed(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& message) {
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError("message missing descriptor");
  }
  CEL_ASSIGN_OR_RETURN(
      auto type,
      ProtoStructType::Resolve(value_factory.type_manager(), *descriptor));
  bool same_descriptors = &type->descriptor() == descriptor;
  if (ABSL_PREDICT_TRUE(same_descriptors)) {
    return value_factory.CreateBorrowedStructValue<
        protobuf_internal::DynamicMemberParsedProtoStructValue>(
        std::move(owner), std::move(type), &message);
  }
  const auto* prototype = type->factory_->GetPrototype(&type->descriptor());
  if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
    return absl::InternalError(absl::StrCat(
        "cel: unable to get prototype for protocol buffer message \"",
        type->name(), "\""));
  }
  std::string serialized;
  if (ABSL_PREDICT_FALSE(!message.SerializePartialToString(&serialized))) {
    return absl::InternalError(
        "cel: failed to serialize protocol buffer message");
  }
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      auto* value = prototype->New(arena);
      if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
        return absl::InternalError(
            "cel: failed to deserialize protocol buffer message");
      }
      return value_factory.CreateBorrowedStructValue<
          protobuf_internal::ArenaDynamicParsedProtoStructValue>(
          std::move(owner), std::move(type), value);
    }
  }
  auto value = absl::WrapUnique(prototype->New());
  if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
    return absl::InternalError(
        "cel: failed to deserialize protocol buffer message");
  }
  auto status_or_message = value_factory.CreateBorrowedStructValue<
      protobuf_internal::HeapDynamicParsedProtoStructValue>(
      std::move(owner), std::move(type), value.get());
  if (ABSL_PREDICT_FALSE(!status_or_message.ok())) {
    return status_or_message.status();
  }
  value.release();
  return std::move(status_or_message).value();
}

absl::StatusOr<Handle<ProtoStructValue>> ProtoStructValue::Create(
    ValueFactory& value_factory, google::protobuf::Message&& message) {
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError("message missing descriptor");
  }
  CEL_ASSIGN_OR_RETURN(
      auto type,
      ProtoStructType::Resolve(value_factory.type_manager(), *descriptor));
  bool same_descriptors = &type->descriptor() == descriptor;
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (ABSL_PREDICT_TRUE(arena != nullptr)) {
      google::protobuf::Message* value;
      if (ABSL_PREDICT_TRUE(same_descriptors)) {
        value = message.New(arena);
        const auto* reflect = message.GetReflection();
        if (ABSL_PREDICT_TRUE(reflect != nullptr)) {
          reflect->Swap(&message, value);
        } else {
          // Fallback to copy.
          value->CopyFrom(message);
        }
      } else {
        const auto* prototype =
            type->factory_->GetPrototype(&type->descriptor());
        if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
          return absl::InternalError(absl::StrCat(
              "cel: unable to get prototype for protocol buffer message \"",
              type->name(), "\""));
        }
        value = prototype->New(arena);
        std::string serialized;
        if (ABSL_PREDICT_FALSE(
                !message.SerializePartialToString(&serialized))) {
          return absl::InternalError(
              "cel: failed to serialize protocol buffer message");
        }
        if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
          return absl::InternalError(
              "cel: failed to deserialize protocol buffer message");
        }
      }
      return value_factory.CreateStructValue<
          protobuf_internal::ArenaDynamicParsedProtoStructValue>(type, value);
    }
  }
  std::unique_ptr<google::protobuf::Message> value;
  if (ABSL_PREDICT_TRUE(same_descriptors)) {
    value = absl::WrapUnique(message.New());
    const auto* reflect = message.GetReflection();
    if (ABSL_PREDICT_TRUE(reflect != nullptr)) {
      reflect->Swap(&message, value.get());
    } else {
      // Fallback to copy.
      value->CopyFrom(message);
    }
  } else {
    const auto* prototype = type->factory_->GetPrototype(&type->descriptor());
    if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
      return absl::InternalError(absl::StrCat(
          "cel: unable to get prototype for protocol buffer message \"",
          type->name(), "\""));
    }
    value = absl::WrapUnique(prototype->New());
    std::string serialized;
    if (ABSL_PREDICT_FALSE(!message.SerializePartialToString(&serialized))) {
      return absl::InternalError(
          "cel: failed to serialize protocol buffer message");
    }
    if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
      return absl::InternalError(
          "cel: failed to deserialize protocol buffer message");
    }
  }
  auto status_or_message = value_factory.CreateStructValue<
      protobuf_internal::HeapDynamicParsedProtoStructValue>(type, value.get());
  if (ABSL_PREDICT_FALSE(!status_or_message.ok())) {
    return status_or_message.status();
  }
  value.release();
  return std::move(status_or_message).value();
}

namespace protobuf_internal {

std::string ParsedProtoStructValue::DebugString(
    const google::protobuf::Message& message) {
  std::string out;
  out.append(message.GetTypeName());
  out.push_back('{');
  const auto* reflect = message.GetReflection();
  if (reflect != nullptr) {
    std::vector<const google::protobuf::FieldDescriptor*> field_descs;
    reflect->ListFields(message, &field_descs);
    auto field_desc = field_descs.begin();
    if (field_desc != field_descs.end()) {
      out.append((*field_desc)->name());
      out.append(": ");
      ProtoDebugString(out, message, reflect, *field_desc);
      ++field_desc;
      for (; field_desc != field_descs.end(); ++field_desc) {
        out.append(", ");
        out.append((*field_desc)->name());
        out.append(": ");
        ProtoDebugString(out, message, reflect, *field_desc);
      }
    }
  }
  out.push_back('}');
  return out;
}

std::string ParsedProtoStructValue::DebugString() const {
  return ParsedProtoStructValue::DebugString(value());
}

size_t ParsedProtoStructValue::field_count() const {
  const auto* reflect = value().GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return 0;
  }
  std::vector<const google::protobuf::FieldDescriptor*> fields;
  reflect->ListFields(value(), &fields);
  return fields.size();
}

google::protobuf::Message* ParsedProtoStructValue::ValuePointer(
    google::protobuf::MessageFactory& message_factory, google::protobuf::Arena* arena) const {
  const auto* desc = value().GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return nullptr;
  }
  const auto* prototype = message_factory.GetPrototype(desc);
  if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  if (ABSL_PREDICT_FALSE(message == nullptr)) {
    return nullptr;
  }
  message->CopyFrom(value());
  return message;
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetFieldByName(
    const GetFieldContext& context, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(
      auto field_type,
      type()->FindFieldByName(context.value_factory().type_manager(), name));
  if (ABSL_PREDICT_FALSE(!field_type)) {
    return runtime_internal::CreateNoSuchFieldError(name);
  }
  return GetField(context, *field_type);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetFieldByNumber(
    const GetFieldContext& context, int64_t number) const {
  CEL_ASSIGN_OR_RETURN(auto field_type,
                       type()->FindFieldByNumber(
                           context.value_factory().type_manager(), number));
  if (ABSL_PREDICT_FALSE(!field_type)) {
    return runtime_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }
  return GetField(context, *field_type);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetField(
    const GetFieldContext& context, const StructType::Field& field) const {
  const auto* reflect = value().GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError("message missing reflection");
  }
  const auto* field_desc =
      static_cast<const google::protobuf::FieldDescriptor*>(field.hint);
  if (field_desc->is_map()) {
    return GetMapField(context, field, *reflect, *field_desc);
  }
  if (field_desc->is_repeated()) {
    return GetRepeatedField(context, field, *reflect, *field_desc);
  }
  return GetSingularField(context, field, *reflect, *field_desc);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetMapField(
    const GetFieldContext& context, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  return context.value_factory().CreateBorrowedMapValue<ParsedProtoMapValue>(
      owner_from_this(), field.type.As<MapType>(), value(), field_desc);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetRepeatedField(
    const GetFieldContext& context, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  switch (field_desc.type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return context.value_factory()
          .CreateBorrowedListValue<ParsedProtoListValue<DoubleValue, double>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<double>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return context.value_factory()
          .CreateBorrowedListValue<ParsedProtoListValue<DoubleValue, float>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<float>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return context.value_factory()
          .CreateBorrowedListValue<ParsedProtoListValue<IntValue, int64_t>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<int64_t>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      return context.value_factory()
          .CreateBorrowedListValue<ParsedProtoListValue<IntValue, int32_t>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<int32_t>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      return context.value_factory()
          .CreateBorrowedListValue<ParsedProtoListValue<UintValue, uint64_t>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<uint64_t>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return context.value_factory()
          .CreateBorrowedListValue<ParsedProtoListValue<UintValue, uint32_t>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<uint32_t>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return context.value_factory()
          .CreateBorrowedListValue<ParsedProtoListValue<BoolValue, bool>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<bool>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return context.value_factory()
          .CreateBorrowedListValue<
              ParsedProtoListValue<StringValue, std::string>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<std::string>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      switch (field.type.As<ListType>()->element()->kind()) {
        case TypeKind::kDuration:
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<DurationValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kTimestamp:
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<TimestampValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kList:
          // google.protobuf.ListValue
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<ListValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kMap:
          // google.protobuf.Struct
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<MapValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kDyn:
          // google.protobuf.Value.
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<DynValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kAny:
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<AnyType, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kBool:
          // google.protobuf.BoolValue, mapped to CEL primitive bool type for
          // list elements.
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<BoolValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kBytes:
          // google.protobuf.BytesValue, mapped to CEL primitive bytes type for
          // list elements.
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<BytesValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kDouble:
          // google.protobuf.{FloatValue,DoubleValue}, mapped to CEL primitive
          // double type for list elements.
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<DoubleValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kInt:
          // google.protobuf.{Int32Value,Int64Value}, mapped to CEL primitive
          // int type for list elements.
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<IntValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kString:
          // google.protobuf.StringValue, mapped to CEL primitive bytes type for
          // list elements.
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<StringValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kUint:
          // google.protobuf.{UInt32Value,UInt64Value}, mapped to CEL primitive
          // uint type for list elements.
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<UintValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        case TypeKind::kStruct:
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<ProtoStructValue, google::protobuf::Message>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                               &field_desc));
        default:
          ABSL_UNREACHABLE();
      }
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return context.value_factory()
          .CreateBorrowedListValue<
              ParsedProtoListValue<BytesValue, std::string>>(
              owner_from_this(), field.type.As<ListType>(),
              reflect.GetRepeatedFieldRef<std::string>(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      switch (field.type.As<ListType>()->element()->kind()) {
        case TypeKind::kNullType:
          return context.value_factory()
              .CreateListValue<ParsedProtoListValue<NullValue>>(
                  field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<int32_t>(value(), &field_desc)
                      .size());
        case TypeKind::kEnum:
          return context.value_factory()
              .CreateBorrowedListValue<
                  ParsedProtoListValue<EnumValue, int32_t>>(
                  owner_from_this(), field.type.As<ListType>(),
                  reflect.GetRepeatedFieldRef<int32_t>(value(), &field_desc));
        default:
          ABSL_UNREACHABLE();
      }
  }
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetSingularField(
    const GetFieldContext& context, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  switch (field_desc.type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return context.value_factory().CreateDoubleValue(
          reflect.GetDouble(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return context.value_factory().CreateDoubleValue(
          reflect.GetFloat(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return context.value_factory().CreateIntValue(
          reflect.GetInt64(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      return context.value_factory().CreateIntValue(
          reflect.GetInt32(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      return context.value_factory().CreateUintValue(
          reflect.GetUInt64(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return context.value_factory().CreateUintValue(
          reflect.GetUInt32(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return context.value_factory().CreateBoolValue(
          reflect.GetBool(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return protobuf_internal::GetBorrowedStringField(
          context.value_factory(), owner_from_this(), value(), &reflect,
          &field_desc);
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      switch (field.type->kind()) {
        case TypeKind::kDuration: {
          CEL_ASSIGN_OR_RETURN(
              auto duration,
              protobuf_internal::AbslDurationFromDurationProto(
                  reflect.GetMessage(value(), &field_desc, type()->factory_)));
          return context.value_factory().CreateUncheckedDurationValue(duration);
        }
        case TypeKind::kTimestamp: {
          CEL_ASSIGN_OR_RETURN(
              auto timestamp,
              protobuf_internal::AbslTimeFromTimestampProto(
                  reflect.GetMessage(value(), &field_desc, type()->factory_)));
          return context.value_factory().CreateUncheckedTimestampValue(
              timestamp);
        }
        case TypeKind::kList:
          // google.protobuf.ListValue
          return protobuf_internal::CreateBorrowedListValue(
              owner_from_this(), context.value_factory(),
              reflect.GetMessage(value(), &field_desc));
        case TypeKind::kMap:
          // google.protobuf.Struct
          return protobuf_internal::CreateBorrowedStruct(
              owner_from_this(), context.value_factory(),
              reflect.GetMessage(value(), &field_desc));
        case TypeKind::kDyn:
          // google.protobuf.Value
          return protobuf_internal::CreateBorrowedValue(
              owner_from_this(), context.value_factory(),
              reflect.GetMessage(value(), &field_desc));
        case TypeKind::kAny:
          // google.protobuf.Any
          return ProtoValue::Create(context.value_factory(),
                                    reflect.GetMessage(value(), &field_desc));
        case TypeKind::kWrapper: {
          if (context.unbox_null_wrapper_types() &&
              !reflect.HasField(value(), &field_desc)) {
            return context.value_factory().GetNullValue();
          }
          switch (field.type.As<WrapperType>()->wrapped()->kind()) {
            case TypeKind::kBool: {
              CEL_ASSIGN_OR_RETURN(
                  auto wrapped,
                  protobuf_internal::UnwrapBoolValueProto(reflect.GetMessage(
                      value(), &field_desc, type()->factory_)));
              return context.value_factory().CreateBoolValue(wrapped);
            }
            case TypeKind::kBytes: {
              CEL_ASSIGN_OR_RETURN(
                  auto wrapped,
                  protobuf_internal::UnwrapBytesValueProto(reflect.GetMessage(
                      value(), &field_desc, type()->factory_)));
              return context.value_factory().CreateBytesValue(
                  std::move(wrapped));
            }
            case TypeKind::kDouble: {
              CEL_ASSIGN_OR_RETURN(
                  auto wrapped,
                  protobuf_internal::UnwrapDoubleValueProto(reflect.GetMessage(
                      value(), &field_desc, type()->factory_)));
              return context.value_factory().CreateDoubleValue(wrapped);
            }
            case TypeKind::kInt: {
              CEL_ASSIGN_OR_RETURN(
                  auto wrapped,
                  protobuf_internal::UnwrapIntValueProto(reflect.GetMessage(
                      value(), &field_desc, type()->factory_)));
              return context.value_factory().CreateIntValue(wrapped);
            }
            case TypeKind::kString: {
              CEL_ASSIGN_OR_RETURN(
                  auto wrapped,
                  protobuf_internal::UnwrapStringValueProto(reflect.GetMessage(
                      value(), &field_desc, type()->factory_)));
              return context.value_factory().CreateUncheckedStringValue(
                  std::move(wrapped));
            }
            case TypeKind::kUint: {
              CEL_ASSIGN_OR_RETURN(
                  auto wrapped,
                  protobuf_internal::UnwrapUIntValueProto(reflect.GetMessage(
                      value(), &field_desc, type()->factory_)));
              return context.value_factory().CreateUintValue(wrapped);
            }
            default:
              // Only these 6 kinds can be wrapped.
              ABSL_UNREACHABLE();
          }
        }
        case TypeKind::kStruct:
          return context.value_factory()
              .CreateBorrowedStructValue<DynamicMemberParsedProtoStructValue>(
                  owner_from_this(), field.type.As<ProtoStructType>(),
                  &(reflect.GetMessage(value(), &field_desc)));
        default:
          ABSL_UNREACHABLE();
      }
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return protobuf_internal::GetBorrowedBytesField(
          context.value_factory(), owner_from_this(), value(), &reflect,
          &field_desc);
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      switch (field.type->kind()) {
        case TypeKind::kNullType:
          return context.value_factory().GetNullValue();
        case TypeKind::kEnum:
          return context.value_factory().CreateEnumValue(
              field.type.As<ProtoEnumType>(),
              reflect.GetEnumValue(value(), &field_desc));
        default:
          ABSL_UNREACHABLE();
      }
  }
}

absl::StatusOr<bool> ParsedProtoStructValue::HasFieldByName(
    const HasFieldContext& context, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(auto field,
                       type()->FindFieldByName(context.type_manager(), name));
  if (ABSL_PREDICT_FALSE(!field.has_value())) {
    return runtime_internal::CreateNoSuchFieldError(name);
  }
  return HasField(context.type_manager(), *field);
}

absl::StatusOr<bool> ParsedProtoStructValue::HasFieldByNumber(
    const HasFieldContext& context, int64_t number) const {
  CEL_ASSIGN_OR_RETURN(
      auto field, type()->FindFieldByNumber(context.type_manager(), number));
  if (ABSL_PREDICT_FALSE(!field.has_value())) {
    return runtime_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }
  return HasField(context.type_manager(), *field);
}

absl::StatusOr<bool> ParsedProtoStructValue::HasField(
    TypeManager& type_manager, const StructType::Field& field) const {
  const auto* field_desc =
      static_cast<const google::protobuf::FieldDescriptor*>(field.hint);
  const auto* reflect = value().GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError("message missing reflection");
  }
  if (field_desc->is_repeated()) {
    return reflect->FieldSize(value(), field_desc) != 0;
  }
  return reflect->HasField(value(), field_desc);
}

class ParsedProtoStructValueFieldIterator final
    : public StructValue::FieldIterator {
 public:
  ParsedProtoStructValueFieldIterator(
      const ParsedProtoStructValue* value,
      std::vector<const google::protobuf::FieldDescriptor*> fields)
      : value_(value), fields_(std::move(fields)) {}

  bool HasNext() override { return index_ < fields_.size(); }

  absl::StatusOr<Field> Next(
      const StructValue::GetFieldContext& context) override {
    if (ABSL_PREDICT_FALSE(index_ >= fields_.size())) {
      return absl::FailedPreconditionError(
          "StructValue::FieldIterator::Next() called when "
          "StructValue::FieldIterator::HasNext() returns false");
    }
    const auto* field = fields_[index_];
    CEL_ASSIGN_OR_RETURN(auto type, value_->type()->FindFieldByNumber(
                                        context.value_factory().type_manager(),
                                        field->number()));
    CEL_ASSIGN_OR_RETURN(auto value,
                         value_->GetField(context, std::move(type).value()));
    ++index_;
    return Field(ParsedProtoStructValue::MakeFieldId(field->number()),
                 std::move(value));
  }

  absl::StatusOr<StructValue::FieldId> NextId(
      const StructValue::GetFieldContext& context) override {
    if (ABSL_PREDICT_FALSE(index_ >= fields_.size())) {
      return absl::FailedPreconditionError(
          "StructValue::FieldIterator::Next() called when "
          "StructValue::FieldIterator::HasNext() returns false");
    }
    return ParsedProtoStructValue::MakeFieldId(fields_[index_++]->number());
  }

 private:
  const ParsedProtoStructValue* const value_;
  const std::vector<const google::protobuf::FieldDescriptor*> fields_;
  size_t index_ = 0;
};

absl::StatusOr<UniqueRef<StructValue::FieldIterator>>
ParsedProtoStructValue::NewFieldIterator(MemoryManager& memory_manager) const {
  const auto* reflect = value().GetReflection();
  std::vector<const google::protobuf::FieldDescriptor*> fields;
  if (ABSL_PREDICT_TRUE(reflect != nullptr)) {
    reflect->ListFields(value(), &fields);
  }
  return MakeUnique<ParsedProtoStructValueFieldIterator>(memory_manager, this,
                                                         std::move(fields));
}

absl::Status ParsedProtoStructValue::CopyTo(google::protobuf::Message& that) const {
  const auto* this_desc = value().GetDescriptor();
  const auto* that_desc = that.GetDescriptor();
  if (ABSL_PREDICT_TRUE(this_desc == that_desc)) {
    that.CopyFrom(value());
    return absl::OkStatus();
  }
  if (this_desc->full_name() == that_desc->full_name()) {
    // Same type, different descriptors. We need to serialize and deserialize.
    absl::Cord serialized;
    if (ABSL_PREDICT_FALSE(!value().SerializeToCord(&serialized))) {
      return absl::InternalError(
          absl::StrCat("failed to serialize protocol buffer message ",
                       this_desc->full_name()));
    }
    if (ABSL_PREDICT_FALSE(!that.ParseFromCord(serialized))) {
      return absl::InternalError(absl::StrCat(
          "failed to parse protocol buffer message ", that_desc->full_name()));
    }
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("cannot copy protocol buffer message ",
                   this_desc->full_name(), " to ", that_desc->full_name()));
}

}  // namespace protobuf_internal

}  // namespace cel::extensions
