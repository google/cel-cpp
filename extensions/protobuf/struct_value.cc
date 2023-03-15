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

#include "extensions/protobuf/struct_value.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/container/btree_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "base/allocator.h"
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
#include "extensions/protobuf/enum_type.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "extensions/protobuf/internal/time.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/struct_type.h"
#include "internal/status_macros.h"
#include "internal/unreachable.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace cel::extensions {

namespace protobuf_internal {

namespace {

class HeapDynamicParsedProtoStructValue final
    : public DynamicParsedProtoStructValue {
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
  static absl::StatusOr<Handle<ProtoStructValue>> Create(
      ValueFactory& value_factory, Handle<StructType> type, const Value* parent,
      const google::protobuf::Message* value);

  const google::protobuf::Message& value() const final { return *value_; }

 protected:
  DynamicMemberParsedProtoStructValue(Handle<StructType> type,
                                      const google::protobuf::Message* value)
      : ParsedProtoStructValue(std::move(type)),
        value_(ABSL_DIE_IF_NULL(value)) {}  // Crash OK

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

class ArenaDynamicMemberParsedProtoStructValue final
    : public DynamicMemberParsedProtoStructValue {
 public:
  ArenaDynamicMemberParsedProtoStructValue(Handle<StructType> type,
                                           const google::protobuf::Message* value)
      : DynamicMemberParsedProtoStructValue(std::move(type), value) {}
};

class ReffedDynamicMemberParsedProtoStructValue final
    : public DynamicMemberParsedProtoStructValue {
 public:
  ReffedDynamicMemberParsedProtoStructValue(Handle<StructType> type,
                                            const Value* parent,
                                            const google::protobuf::Message* value)
      : DynamicMemberParsedProtoStructValue(std::move(type), value),
        parent_(parent) {
    base_internal::Metadata::Ref(*parent_);
  }

  ~ReffedDynamicMemberParsedProtoStructValue() override {
    base_internal::ValueMetadata::Unref(*parent_);
  }

 private:
  const Value* const parent_;
};

absl::StatusOr<Handle<ProtoStructValue>>
DynamicMemberParsedProtoStructValue::Create(ValueFactory& value_factory,
                                            Handle<StructType> type,
                                            const Value* parent,
                                            const google::protobuf::Message* value) {
  if (parent != nullptr &&
      base_internal::Metadata::IsReferenceCounted(*parent)) {
    return value_factory
        .CreateStructValue<ReffedDynamicMemberParsedProtoStructValue>(
            std::move(type), parent, value);
  }
  return value_factory
      .CreateStructValue<ArenaDynamicMemberParsedProtoStructValue>(
          std::move(type), value);
}

}  // namespace

}  // namespace protobuf_internal

std::unique_ptr<google::protobuf::Message> ProtoStructValue::value(
    google::protobuf::MessageFactory& message_factory) const {
  return absl::WrapUnique(ValuePointer(message_factory, nullptr));
}

std::unique_ptr<google::protobuf::Message> ProtoStructValue::value() const {
  return absl::WrapUnique(
      ValuePointer(*ABSL_DIE_IF_NULL(  // Crash OK
                       google::protobuf::MessageFactory::generated_factory()),
                   nullptr));
}

google::protobuf::Message* ProtoStructValue::value(
    google::protobuf::Arena& arena, google::protobuf::MessageFactory& message_factory) const {
  return ValuePointer(message_factory, &arena);
}

google::protobuf::Message* ProtoStructValue::value(google::protobuf::Arena& arena) const {
  return ValuePointer(*ABSL_DIE_IF_NULL(  // Crash OK
                          google::protobuf::MessageFactory::generated_factory()),
                      &arena);
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

template <typename T, typename P>
class ParsedProtoListValue;
template <typename T, typename P>
class ArenaParsedProtoListValue;
template <typename T, typename P>
class ReffedParsedProtoListValue;

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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    return value_factory.CreateBoolValue(fields_.Get(static_cast<int>(index)));
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    return value_factory.CreateIntValue(fields_.Get(static_cast<int>(index)));
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    return value_factory.CreateUintValue(fields_.Get(static_cast<int>(index)));
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    return value_factory.CreateDoubleValue(
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    // Proto does not provide a zero copy interface for accessing repeated bytes
    // fields.
    return value_factory.CreateBytesValue(fields_.Get(static_cast<int>(index)));
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    // Proto does not provide a zero copy interface for accessing repeated
    // string fields.
    return value_factory.CreateUncheckedStringValue(
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    CEL_ASSIGN_OR_RETURN(
        auto duration,
        protobuf_internal::AbslDurationFromDurationProto(
            fields_.Get(static_cast<int>(index), scratch.get())));
    scratch.reset();
    return value_factory.CreateUncheckedDurationValue(duration);
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    CEL_ASSIGN_OR_RETURN(
        auto time, protobuf_internal::AbslTimeFromTimestampProto(
                       fields_.Get(static_cast<int>(index), scratch.get())));
    scratch.reset();
    return value_factory.CreateUncheckedTimestampValue(time);
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    return value_factory.CreateEnumValue(type()->element().As<EnumType>(),
                                         fields_.Get(static_cast<int>(index)));
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    std::unique_ptr<google::protobuf::Message> scratch(fields_.NewMessage());
    const auto& field = fields_.Get(static_cast<int>(index), scratch.get());
    if (&field != scratch.get()) {
      // Scratch was not used, we can avoid copying.
      scratch.reset();
      return protobuf_internal::DynamicMemberParsedProtoStructValue::Create(
          value_factory, type()->element().As<StructType>(), this, &field);
    }
    if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
      auto* arena =
          ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
      if (ABSL_PREDICT_TRUE(arena != nullptr)) {
        // We are using google::protobuf::Arena, but fields_.NewMessage() allocates on the
        // heap. Copy the message into the arena to avoid the extra bookkeeping.
        auto* message = field.New(arena);
        message->CopyFrom(*scratch);
        scratch.reset();
        return value_factory.CreateStructValue<
            protobuf_internal::ArenaDynamicParsedProtoStructValue>(
            type()->element().As<ProtoStructType>(), message);
      }
    }
    return value_factory.CreateStructValue<
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

template <typename T, typename P>
class ArenaParsedProtoListValue final : public ParsedProtoListValue<T, P> {
 public:
  using ParsedProtoListValue<T, P>::ParsedProtoListValue;
};

template <typename T, typename P>
class ReffedParsedProtoListValue final : public ParsedProtoListValue<T, P> {
 public:
  ReffedParsedProtoListValue(Handle<ListType> type,
                             google::protobuf::RepeatedFieldRef<P> fields,
                             const Value* owner)
      : ParsedProtoListValue<T, P>(std::move(type), std::move(fields)),
        owner_(owner) {
    cel::base_internal::ValueMetadata::Ref(*owner_);
  }

  ~ReffedParsedProtoListValue() override {
    cel::base_internal::ValueMetadata::Unref(*owner_);
  }

 private:
  const Value* owner_;
};

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
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
      const auto* desc = field.enum_type();
      const auto* value_desc = desc->FindValueByNumber(value.GetEnumValue());
      out.append(desc->full_name());
      if (value_desc != nullptr) {
        out.push_back('.');
        out.append(value_desc->name());
      } else {
        out.push_back('(');
        out.append(absl::StrCat(value.GetEnumValue()));
        out.push_back(')');
      }
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      out.append(protobuf_internal::ParsedProtoStructValue::DebugString(
          value.GetMessageValue()));
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

absl::StatusOr<Handle<Value>> FromProtoMapKey(ValueFactory& value_factory,
                                              const google::protobuf::MapKey& key,
                                              const Value* owner) {
  switch (key.type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return value_factory.CreateIntValue(key.GetInt64Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return value_factory.CreateIntValue(key.GetInt32Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return value_factory.CreateUintValue(key.GetUInt64Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return value_factory.CreateUintValue(key.GetUInt32Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:

      if (cel::base_internal::Metadata::IsReferenceCounted(*owner)) {
        return cel::base_internal::ValueFactoryAccess::CreateMemberStringValue(
            value_factory, key.GetStringValue(), owner);
      }
      return interop_internal::CreateStringValueFromView(key.GetStringValue());
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return value_factory.CreateBoolValue(key.GetBoolValue());
    default:
      // Unreachable because protobuf is extremely unlikely to introduce
      // additional supported key types.
      ABSL_UNREACHABLE();
  }
}

// Transform Value into MapKey. Requires that value is compatible with protocol
// buffer map key.
bool ToProtoMapKey(google::protobuf::MapKey& key, const Handle<Value>& value,
                   const google::protobuf::FieldDescriptor& field) {
  switch (value->kind()) {
    case Kind::kBool:
      key.SetBoolValue(value.As<BoolValue>()->value());
      break;
    case Kind::kInt: {
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
    case Kind::kUint: {
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
    case Kind::kString:
      key.SetStringValue(value.As<StringValue>()->ToString());
      break;
    default:
      // Unreachable because protobuf is extremely unlikely to introduce
      // additional supported key types.
      ABSL_UNREACHABLE();
  }
  return true;
}

absl::StatusOr<Handle<Value>> FromProtoMapValue(
    ValueFactory& value_factory, const google::protobuf::MapValueConstRef& value,
    const google::protobuf::FieldDescriptor& field, const Value* owner) {
  const auto* value_desc = field.message_type()->map_value();
  switch (value_desc->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return value_factory.CreateBoolValue(value.GetBoolValue());
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return value_factory.CreateIntValue(value.GetInt64Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return value_factory.CreateIntValue(value.GetInt32Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return value_factory.CreateUintValue(value.GetUInt64Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return value_factory.CreateUintValue(value.GetUInt32Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return value_factory.CreateDoubleValue(value.GetFloatValue());
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return value_factory.CreateDoubleValue(value.GetDoubleValue());
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      if (value_desc->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        if (cel::base_internal::Metadata::IsReferenceCounted(*owner)) {
          return cel::base_internal::ValueFactoryAccess::CreateMemberBytesValue(
              value_factory, value.GetStringValue(), owner);
        }
        return interop_internal::CreateBytesValueFromView(
            value.GetStringValue());
      } else {
        if (cel::base_internal::Metadata::IsReferenceCounted(*owner)) {
          return cel::base_internal::ValueFactoryAccess::
              CreateMemberStringValue(value_factory, value.GetStringValue(),
                                      owner);
        }
        return interop_internal::CreateStringValueFromView(
            value.GetStringValue());
      }
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
      CEL_ASSIGN_OR_RETURN(auto type,
                           ProtoEnumType::Resolve(value_factory.type_manager(),
                                                  *value_desc->enum_type()));
      return value_factory.CreateEnumValue(std::move(type),
                                           value.GetEnumValue());
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
      CEL_ASSIGN_OR_RETURN(
          auto type, ProtoStructType::Resolve(value_factory.type_manager(),
                                              *value_desc->message_type()));
      return protobuf_internal::DynamicMemberParsedProtoStructValue::Create(
          value_factory, std::move(type), owner, &value.GetMessageValue());
    }
  }
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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const final {
    return FromProtoMapKey(value_factory, keys_[index], this);
  }

 private:
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ParsedProtoMapValueKeysList>();
  }

  const std::vector<google::protobuf::MapKey, Allocator<google::protobuf::MapKey>> keys_;
};

class ArenaParsedProtoMapValueKeysList final
    : public ParsedProtoMapValueKeysList {
 public:
  using ParsedProtoMapValueKeysList::ParsedProtoMapValueKeysList;
};

class ReffedParsedProtoMapValueKeysList final
    : public ParsedProtoMapValueKeysList {
 public:
  ReffedParsedProtoMapValueKeysList(
      Handle<ListType> type,
      std::vector<google::protobuf::MapKey, Allocator<google::protobuf::MapKey>> keys,
      const Value* owner)
      : ParsedProtoMapValueKeysList(std::move(type), std::move(keys)),
        owner_(owner) {
    cel::base_internal::ValueMetadata::Ref(*owner_);
  }

  ~ReffedParsedProtoMapValueKeysList() override {
    cel::base_internal::ValueMetadata::Unref(*owner_);
  }

 private:
  const Value* const owner_;
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
      ValueFactory& value_factory, const Handle<Value>& key) const final {
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
    return FromProtoMapValue(value_factory, proto_value, field_, this);
  }

  absl::StatusOr<bool> Has(const Handle<Value>& key) const final {
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
      ValueFactory& value_factory) const final {
    CEL_ASSIGN_OR_RETURN(
        auto list_type,
        value_factory.type_factory().CreateListType(type()->key()));
    std::vector<google::protobuf::MapKey, Allocator<google::protobuf::MapKey>> keys(
        Allocator<google::protobuf::MapKey>(value_factory.memory_manager()));
    keys.reserve(size());
    auto begin = protobuf_internal::MapBegin(reflection(), message_, field_);
    auto end = protobuf_internal::MapEnd(reflection(), message_, field_);
    for (; begin != end; ++begin) {
      keys.push_back(begin.GetKey());
    }
    if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
      return value_factory.CreateListValue<ReffedParsedProtoMapValueKeysList>(
          std::move(list_type), std::move(keys), this);
    }
    return value_factory.CreateListValue<ArenaParsedProtoMapValueKeysList>(
        std::move(list_type), std::move(keys));
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

class ArenaParsedProtoMapValue final : public ParsedProtoMapValue {
 public:
  using ParsedProtoMapValue::ParsedProtoMapValue;
};

class ReffedParsedProtoMapValue final : public ParsedProtoMapValue {
 public:
  ReffedParsedProtoMapValue(Handle<MapType> type,
                            const google::protobuf::Message& message,
                            const google::protobuf::FieldDescriptor& field,
                            const Value* owner)
      : ParsedProtoMapValue(std::move(type), message, field), owner_(owner) {
    cel::base_internal::ValueMetadata::Ref(*owner_);
  }

  ~ReffedParsedProtoMapValue() override {
    cel::base_internal::ValueMetadata::Unref(*owner_);
  }

 private:
  const Value* owner_;
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
      if (field_desc->message_type()->full_name() ==
          "google.protobuf.Duration") {
        out.append(DurationValueDebugStringFromProto(
            reflect->GetMessage(message, field_desc)));
        break;
      }
      if (field_desc->message_type()->full_name() ==
          "google.protobuf.Timestamp") {
        out.append(TimestampValueDebugStringFromProto(
            reflect->GetMessage(message, field_desc)));
        break;
      }
      out.append(protobuf_internal::ParsedProtoStructValue::DebugString(
          reflect->GetMessage(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      std::string scratch;
      out.append(BytesValue::DebugString(
          reflect->GetStringReference(message, field_desc, &scratch)));
    } break;
    case google::protobuf::FieldDescriptor::TYPE_ENUM: {
      const auto* desc = reflect->GetEnum(message, field_desc);
      out.append(field_desc->enum_type()->full_name());
      if (ABSL_PREDICT_TRUE(desc != nullptr)) {
        out.push_back('.');
        out.append(desc->name());
      } else {
        // Fallback when the number doesn't have a corresponding symbol,
        // potentially due to having a different version of the descriptor.
        out.push_back('(');
        out.append(
            IntValue::DebugString(reflect->GetEnumValue(message, field_desc)));
        out.push_back(')');
      }
    } break;
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
      using DebugStringer = std::string (*)(const google::protobuf::Message&);
      DebugStringer debug_stringer;
      if (field_desc->message_type()->full_name() ==
          "google.protobuf.Duration") {
        debug_stringer = DurationValueDebugStringFromProto;
      } else if (field_desc->message_type()->full_name() ==
                 "google.protobuf.Timestamp") {
        debug_stringer = TimestampValueDebugStringFromProto;
      } else {
        debug_stringer = protobuf_internal::ParsedProtoStructValue::DebugString;
      }
      auto fields =
          reflect->GetRepeatedFieldRef<google::protobuf::Message>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append((*debug_stringer)(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append((*debug_stringer)(*field));
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
      const auto* desc = field_desc->enum_type();
      auto fields = reflect->GetRepeatedFieldRef<int32_t>(message, field_desc);
      auto field = fields.begin();
      if (field != fields.end()) {
        out.append(desc->full_name());
        if (const auto* value_desc = desc->FindValueByNumber(*field);
            value_desc != nullptr) {
          out.push_back('.');
          out.append(value_desc->name());
        } else {
          out.push_back('(');
          out.append(IntValue::DebugString(*field));
          out.push_back(')');
        }
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(desc->full_name());
          if (const auto* value_desc = desc->FindValueByNumber(*field);
              value_desc != nullptr) {
            out.push_back('.');
            out.append(value_desc->name());
          } else {
            out.push_back('(');
            out.append(IntValue::DebugString(*field));
            out.push_back(')');
          }
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
    ValueFactory& value_factory, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(
      auto field_type,
      type()->FindField(value_factory.type_manager(), FieldId(name)));
  if (ABSL_PREDICT_FALSE(!field_type)) {
    return interop_internal::CreateNoSuchFieldError(name);
  }
  return GetField(value_factory, *field_type);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetFieldByNumber(
    ValueFactory& value_factory, int64_t number) const {
  CEL_ASSIGN_OR_RETURN(
      auto field_type,
      type()->FindField(value_factory.type_manager(), FieldId(number)));
  if (ABSL_PREDICT_FALSE(!field_type)) {
    return interop_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }
  return GetField(value_factory, *field_type);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetField(
    ValueFactory& value_factory, const StructType::Field& field) const {
  const auto* reflect = value().GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError("message missing reflection");
  }
  const auto* field_desc =
      static_cast<const google::protobuf::FieldDescriptor*>(field.hint);
  if (field_desc->is_map()) {
    return GetMapField(value_factory, field, *reflect, *field_desc);
  }
  if (field_desc->is_repeated()) {
    return GetRepeatedField(value_factory, field, *reflect, *field_desc);
  }
  return GetSingularField(value_factory, field, *reflect, *field_desc);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetMapField(
    ValueFactory& value_factory, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
    return value_factory.CreateMapValue<ReffedParsedProtoMapValue>(
        field.type.As<MapType>(), value(), field_desc, this);
  } else {
    return value_factory.CreateMapValue<ArenaParsedProtoMapValue>(
        field.type.As<MapType>(), value(), field_desc);
  }
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetRepeatedField(
    ValueFactory& value_factory, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  switch (field_desc.type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<DoubleValue, double>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<double>(value(), &field_desc),
                this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<DoubleValue, double>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<double>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<DoubleValue, float>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<float>(value(), &field_desc), this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<DoubleValue, float>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<float>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<IntValue, int64_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<int64_t>(value(), &field_desc),
                this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<IntValue, int64_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<int64_t>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<IntValue, int32_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<int32_t>(value(), &field_desc),
                this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<IntValue, int32_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<int32_t>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<UintValue, uint64_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<uint64_t>(value(), &field_desc),
                this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<UintValue, uint64_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<uint64_t>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<UintValue, uint32_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<uint32_t>(value(), &field_desc),
                this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<UintValue, uint32_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<uint32_t>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<BoolValue, bool>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<bool>(value(), &field_desc), this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<BoolValue, bool>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<bool>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory.CreateListValue<
            ReffedParsedProtoListValue<StringValue, std::string>>(
            field.type.As<ListType>(),
            reflect.GetRepeatedFieldRef<std::string>(value(), &field_desc),
            this);
      } else {
        return value_factory.CreateListValue<
            ArenaParsedProtoListValue<StringValue, std::string>>(
            field.type.As<ListType>(),
            reflect.GetRepeatedFieldRef<std::string>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      switch (field.type.As<ListType>()->element()->kind()) {
        case Kind::kDuration:
          if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
            return value_factory.CreateListValue<
                ReffedParsedProtoListValue<DurationValue, google::protobuf::Message>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                             &field_desc),
                this);
          } else {
            return value_factory.CreateListValue<
                ArenaParsedProtoListValue<DurationValue, google::protobuf::Message>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                             &field_desc));
          }
        case Kind::kTimestamp:
          if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
            return value_factory.CreateListValue<
                ReffedParsedProtoListValue<TimestampValue, google::protobuf::Message>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                             &field_desc),
                this);
          } else {
            return value_factory.CreateListValue<
                ArenaParsedProtoListValue<TimestampValue, google::protobuf::Message>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                             &field_desc));
          }
        case Kind::kStruct:
          if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
            return value_factory.CreateListValue<
                ReffedParsedProtoListValue<ProtoStructValue, google::protobuf::Message>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                             &field_desc),
                this);
          } else {
            return value_factory.CreateListValue<
                ArenaParsedProtoListValue<ProtoStructValue, google::protobuf::Message>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(),
                                                             &field_desc));
          }
        default:
          cel::internal::unreachable();
      }
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory.CreateListValue<
            ReffedParsedProtoListValue<BytesValue, std::string>>(
            field.type.As<ListType>(),
            reflect.GetRepeatedFieldRef<std::string>(value(), &field_desc),
            this);
      } else {
        return value_factory.CreateListValue<
            ArenaParsedProtoListValue<BytesValue, std::string>>(
            field.type.As<ListType>(),
            reflect.GetRepeatedFieldRef<std::string>(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory
            .CreateListValue<ReffedParsedProtoListValue<EnumValue, int32_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<int32_t>(value(), &field_desc),
                this);
      } else {
        return value_factory
            .CreateListValue<ArenaParsedProtoListValue<EnumValue, int32_t>>(
                field.type.As<ListType>(),
                reflect.GetRepeatedFieldRef<int32_t>(value(), &field_desc));
      }
  }
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetSingularField(
    ValueFactory& value_factory, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  switch (field_desc.type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return value_factory.CreateDoubleValue(
          reflect.GetDouble(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return value_factory.CreateDoubleValue(
          reflect.GetFloat(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return value_factory.CreateIntValue(
          reflect.GetInt64(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      return value_factory.CreateIntValue(
          reflect.GetInt32(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      return value_factory.CreateUintValue(
          reflect.GetUInt64(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return value_factory.CreateUintValue(
          reflect.GetUInt32(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return value_factory.CreateBoolValue(
          reflect.GetBool(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      if (field_desc.options().ctype() == google::protobuf::FieldOptions::CORD &&
          !field_desc.is_extension()) {
        return value_factory.CreateStringValue(
            reflect.GetCord(value(), &field_desc));
      } else if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return base_internal::ValueFactoryAccess::CreateMemberStringValue(
            value_factory, reflect.GetStringView(value(), &field_desc), this);
      } else {
        return cel::interop_internal::CreateStringValueFromView(
            reflect.GetStringView(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      switch (field.type->kind()) {
        case Kind::kDuration: {
          CEL_ASSIGN_OR_RETURN(
              auto duration,
              protobuf_internal::AbslDurationFromDurationProto(
                  reflect.GetMessage(value(), &field_desc, type()->factory_)));
          return value_factory.CreateUncheckedDurationValue(duration);
        }
        case Kind::kTimestamp: {
          CEL_ASSIGN_OR_RETURN(
              auto timestamp,
              protobuf_internal::AbslTimeFromTimestampProto(
                  reflect.GetMessage(value(), &field_desc, type()->factory_)));
          return value_factory.CreateUncheckedTimestampValue(timestamp);
        }
        case Kind::kStruct:
          return DynamicMemberParsedProtoStructValue::Create(
              value_factory, field.type.As<ProtoStructType>(), this,
              &(reflect.GetMessage(value(), &field_desc)));
        default:
          cel::internal::unreachable();
      }
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      if (field_desc.options().ctype() == google::protobuf::FieldOptions::CORD &&
          !field_desc.is_extension()) {
        return value_factory.CreateBytesValue(
            reflect.GetCord(value(), &field_desc));
      } else if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return base_internal::ValueFactoryAccess::CreateMemberBytesValue(
            value_factory, reflect.GetStringView(value(), &field_desc), this);
      } else {
        return cel::interop_internal::CreateBytesValueFromView(
            reflect.GetStringView(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return value_factory.CreateEnumValue(
          field.type.As<EnumType>(),
          reflect.GetEnumValue(value(), &field_desc));
  }
}

absl::StatusOr<bool> ParsedProtoStructValue::HasFieldByName(
    TypeManager& type_manager, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(auto field,
                       type()->FindField(type_manager, FieldId(name)));
  if (ABSL_PREDICT_FALSE(!field.has_value())) {
    return interop_internal::CreateNoSuchFieldError(name);
  }
  return HasField(type_manager, *field);
}

absl::StatusOr<bool> ParsedProtoStructValue::HasFieldByNumber(
    TypeManager& type_manager, int64_t number) const {
  CEL_ASSIGN_OR_RETURN(auto field,
                       type()->FindField(type_manager, FieldId(number)));
  if (ABSL_PREDICT_FALSE(!field.has_value())) {
    return interop_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }
  return HasField(type_manager, *field);
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

}  // namespace protobuf_internal

}  // namespace cel::extensions
