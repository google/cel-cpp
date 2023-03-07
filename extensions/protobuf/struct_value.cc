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
#include "absl/base/optimization.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
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
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace cel::extensions {

namespace proto_internal {

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

}  // namespace proto_internal

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
      out.append(ProtoStructValue::DebugString(*field));
      ++field;
      for (; field != fields_.end(); ++field) {
        out.append(", ");
        out.append(ProtoStructValue::DebugString(*field));
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
      return proto_internal::DynamicMemberParsedProtoStructValue::Create(
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
            proto_internal::ArenaDynamicParsedProtoStructValue>(
            type()->element().As<ProtoStructType>(), message);
      }
    }
    return value_factory
        .CreateStructValue<proto_internal::HeapDynamicParsedProtoStructValue>(
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
      out.append(ProtoStructValue::DebugString(
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

void ProtoDebugStringMap(std::string& out, const google::protobuf::Message& message,
                         const google::protobuf::Reflection* reflect,
                         const google::protobuf::FieldDescriptor* field_desc) {
  out.append("**unimplemented**");
  static_cast<void>(message);
  static_cast<void>(reflect);
  static_cast<void>(field_desc);
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
        out.append(ProtoStructValue::DebugString(*field));
        ++field;
        for (; field != fields.end(); ++field) {
          out.append(", ");
          out.append(ProtoStructValue::DebugString(*field));
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
          proto_internal::ArenaDynamicParsedProtoStructValue>(type, value);
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
  auto status_or_message =
      value_factory
          .CreateStructValue<proto_internal::HeapDynamicParsedProtoStructValue>(
              type, value.get());
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
          proto_internal::ArenaDynamicParsedProtoStructValue>(type, value);
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
  auto status_or_message =
      value_factory
          .CreateStructValue<proto_internal::HeapDynamicParsedProtoStructValue>(
              type, value.get());
  if (ABSL_PREDICT_FALSE(!status_or_message.ok())) {
    return status_or_message.status();
  }
  value.release();
  return std::move(status_or_message).value();
}

std::string ProtoStructValue::DebugString(const google::protobuf::Message& message) {
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

namespace proto_internal {

std::string ParsedProtoStructValue::DebugString() const {
  return ProtoStructValue::DebugString(value());
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
  return absl::UnimplementedError(
      "cel: access to protocol buffer message map fields is not yet "
      "implemented");
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
      if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return value_factory.CreateListValue<
            ReffedParsedProtoListValue<ProtoStructValue, google::protobuf::Message>>(
            field.type.As<ListType>(),
            reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(), &field_desc),
            this);
      } else {
        return value_factory.CreateListValue<
            ArenaParsedProtoListValue<ProtoStructValue, google::protobuf::Message>>(
            field.type.As<ListType>(),
            reflect.GetRepeatedFieldRef<google::protobuf::Message>(value(), &field_desc));
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
      return DynamicMemberParsedProtoStructValue::Create(
          value_factory, field.type.As<ProtoStructType>(), this,
          &(reflect.GetMessage(value(), &field_desc)));
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

}  // namespace proto_internal

}  // namespace cel::extensions
