// Copyright 2022 Google LLC
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

#include "eval/public/structs/cel_proto_wrap_util.h"

#include <math.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/message.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/protobuf_value_factory.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/overflow.h"
#include "internal/proto_time_encoding.h"

namespace google::api::expr::runtime::internal {

namespace {

using cel::internal::DecodeDuration;
using cel::internal::DecodeTime;
using cel::internal::EncodeTime;
using google::protobuf::Any;
using google::protobuf::BoolValue;
using google::protobuf::BytesValue;
using google::protobuf::DoubleValue;
using google::protobuf::Duration;
using google::protobuf::FloatValue;
using google::protobuf::Int32Value;
using google::protobuf::Int64Value;
using google::protobuf::ListValue;
using google::protobuf::StringValue;
using google::protobuf::Struct;
using google::protobuf::Timestamp;
using google::protobuf::UInt32Value;
using google::protobuf::UInt64Value;
using google::protobuf::Value;
using google::protobuf::Arena;
using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::Message;
using google::protobuf::MessageFactory;

// kMaxIntJSON is defined as the Number.MAX_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMaxIntJSON = (1ll << 53) - 1;

// kMinIntJSON is defined as the Number.MIN_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMinIntJSON = -kMaxIntJSON;

// Forward declaration for google.protobuf.Value
google::protobuf::Message* MessageFromValue(const CelValue& value, Value* json);

// IsJSONSafe indicates whether the int is safely representable as a floating
// point value in JSON.
static bool IsJSONSafe(int64_t i) {
  return i >= kMinIntJSON && i <= kMaxIntJSON;
}

// IsJSONSafe indicates whether the uint is safely representable as a floating
// point value in JSON.
static bool IsJSONSafe(uint64_t i) {
  return i <= static_cast<uint64_t>(kMaxIntJSON);
}

// Map implementation wrapping google.protobuf.ListValue
class DynamicList : public CelList {
 public:
  DynamicList(const ListValue* values, ProtobufValueFactory factory,
              Arena* arena)
      : arena_(arena), factory_(std::move(factory)), values_(values) {}

  CelValue operator[](int index) const override;

  // List size
  int size() const override { return values_->values_size(); }

 private:
  Arena* arena_;
  ProtobufValueFactory factory_;
  const ListValue* values_;
};

// Map implementation wrapping google.protobuf.Struct.
class DynamicMap : public CelMap {
 public:
  DynamicMap(const Struct* values, ProtobufValueFactory factory, Arena* arena)
      : arena_(arena),
        factory_(std::move(factory)),
        values_(values),
        key_list_(values) {}

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    CelValue::StringHolder str_key;
    if (!key.GetValue(&str_key)) {
      // Not a string key.
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", CelValue::TypeName(key.type()), "'"));
    }

    return values_->fields().contains(std::string(str_key.value()));
  }

  absl::optional<CelValue> operator[](CelValue key) const override;

  int size() const override { return values_->fields_size(); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return &key_list_;
  }

 private:
  // List of keys in Struct.fields map.
  // It utilizes lazy initialization, to avoid performance penalties.
  class DynamicMapKeyList : public CelList {
   public:
    explicit DynamicMapKeyList(const Struct* values)
        : values_(values), keys_(), initialized_(false) {}

    // Index access
    CelValue operator[](int index) const override {
      CheckInit();
      return keys_[index];
    }

    // List size
    int size() const override {
      CheckInit();
      return values_->fields_size();
    }

   private:
    void CheckInit() const {
      absl::MutexLock lock(&mutex_);
      if (!initialized_) {
        for (const auto& it : values_->fields()) {
          keys_.push_back(CelValue::CreateString(&it.first));
        }
        initialized_ = true;
      }
    }

    const Struct* values_;
    mutable absl::Mutex mutex_;
    mutable std::vector<CelValue> keys_;
    mutable bool initialized_;
  };

  Arena* arena_;
  ProtobufValueFactory factory_;
  const Struct* values_;
  const DynamicMapKeyList key_list_;
};

// ValueFactory provides ValueFromMessage(....) function family.
// Functions of this family create CelValue object from specific subtypes of
// protobuf message.
class ValueFactory {
 public:
  ValueFactory(const ProtobufValueFactory& factory, google::protobuf::Arena* arena)
      : factory_(factory), arena_(arena) {}

  CelValue ValueFromMessage(const Duration* duration) {
    return CelValue::CreateDuration(DecodeDuration(*duration));
  }

  CelValue ValueFromMessage(const Timestamp* timestamp) {
    return CelValue::CreateTimestamp(DecodeTime(*timestamp));
  }

  CelValue ValueFromMessage(const ListValue* list_values) {
    return CelValue::CreateList(
        Arena::Create<DynamicList>(arena_, list_values, factory_, arena_));
  }

  CelValue ValueFromMessage(const Struct* struct_value) {
    return CelValue::CreateMap(
        Arena::Create<DynamicMap>(arena_, struct_value, factory_, arena_));
  }

  CelValue ValueFromMessage(const Any* any_value,
                            const DescriptorPool* descriptor_pool,
                            MessageFactory* message_factory) {
    auto type_url = any_value->type_url();
    auto pos = type_url.find_last_of('/');
    if (pos == absl::string_view::npos) {
      // TODO(issues/25) What error code?
      // Malformed type_url
      return CreateErrorValue(arena_, "Malformed type_url string");
    }

    std::string full_name = std::string(type_url.substr(pos + 1));
    const Descriptor* nested_descriptor =
        descriptor_pool->FindMessageTypeByName(full_name);

    if (nested_descriptor == nullptr) {
      // Descriptor not found for the type
      // TODO(issues/25) What error code?
      return CreateErrorValue(arena_, "Descriptor not found");
    }

    const Message* prototype = message_factory->GetPrototype(nested_descriptor);
    if (prototype == nullptr) {
      // Failed to obtain prototype for the descriptor
      // TODO(issues/25) What error code?
      return CreateErrorValue(arena_, "Prototype not found");
    }

    Message* nested_message = prototype->New(arena_);
    if (!any_value->UnpackTo(nested_message)) {
      // Failed to unpack.
      // TODO(issues/25) What error code?
      return CreateErrorValue(arena_, "Failed to unpack Any into message");
    }

    return UnwrapMessageToValue(nested_message, factory_, arena_);
  }

  CelValue ValueFromMessage(const Any* any_value) {
    return ValueFromMessage(any_value, DescriptorPool::generated_pool(),
                            MessageFactory::generated_factory());
  }

  CelValue ValueFromMessage(const BoolValue* wrapper) {
    return CelValue::CreateBool(wrapper->value());
  }

  CelValue ValueFromMessage(const Int32Value* wrapper) {
    return CelValue::CreateInt64(wrapper->value());
  }

  CelValue ValueFromMessage(const UInt32Value* wrapper) {
    return CelValue::CreateUint64(wrapper->value());
  }

  CelValue ValueFromMessage(const Int64Value* wrapper) {
    return CelValue::CreateInt64(wrapper->value());
  }

  CelValue ValueFromMessage(const UInt64Value* wrapper) {
    return CelValue::CreateUint64(wrapper->value());
  }

  CelValue ValueFromMessage(const FloatValue* wrapper) {
    return CelValue::CreateDouble(wrapper->value());
  }

  CelValue ValueFromMessage(const DoubleValue* wrapper) {
    return CelValue::CreateDouble(wrapper->value());
  }

  CelValue ValueFromMessage(const StringValue* wrapper) {
    return CelValue::CreateString(&wrapper->value());
  }

  CelValue ValueFromMessage(const BytesValue* wrapper) {
    // BytesValue stores value as Cord
    return CelValue::CreateBytes(
        Arena::Create<std::string>(arena_, std::string(wrapper->value())));
  }

  CelValue ValueFromMessage(const Value* value) {
    switch (value->kind_case()) {
      case Value::KindCase::kNullValue:
        return CelValue::CreateNull();
      case Value::KindCase::kNumberValue:
        return CelValue::CreateDouble(value->number_value());
      case Value::KindCase::kStringValue:
        return CelValue::CreateString(&value->string_value());
      case Value::KindCase::kBoolValue:
        return CelValue::CreateBool(value->bool_value());
      case Value::KindCase::kStructValue:
        return UnwrapMessageToValue(&value->struct_value(), factory_, arena_);
      case Value::KindCase::kListValue:
        return UnwrapMessageToValue(&value->list_value(), factory_, arena_);
      default:
        return CelValue::CreateNull();
    }
  }

 private:
  const ProtobufValueFactory& factory_;
  google::protobuf::Arena* arena_;
};

// Class makes CelValue from generic protobuf Message.
// It holds a registry of CelValue factories for specific subtypes of Message.
// If message does not match any of types stored in registry, generic
// message-containing CelValue is created.
class ValueFromMessageMaker {
 public:
  template <class MessageType>
  static CelValue CreateWellknownTypeValue(const google::protobuf::Message* msg,
                                           const ProtobufValueFactory& factory,
                                           Arena* arena) {
    const MessageType* message =
        google::protobuf::DynamicCastToGenerated<const MessageType>(msg);
    if (message == nullptr) {
      auto message_copy = Arena::CreateMessage<MessageType>(arena);
      if (MessageType::descriptor() == msg->GetDescriptor()) {
        message_copy->CopyFrom(*msg);
        message = message_copy;
      } else {
        // message of well-known type but from a descriptor pool other than the
        // generated one.
        std::string serialized_msg;
        if (msg->SerializeToString(&serialized_msg) &&
            message_copy->ParseFromString(serialized_msg)) {
          message = message_copy;
        }
      }
    }
    return ValueFactory(factory, arena).ValueFromMessage(message);
  }

  static absl::optional<CelValue> CreateValue(
      const google::protobuf::Message* message, const ProtobufValueFactory& factory,
      Arena* arena) {
    switch (message->GetDescriptor()->well_known_type()) {
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
        return CreateWellknownTypeValue<DoubleValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
        return CreateWellknownTypeValue<FloatValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
        return CreateWellknownTypeValue<Int64Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
        return CreateWellknownTypeValue<UInt64Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
        return CreateWellknownTypeValue<Int32Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
        return CreateWellknownTypeValue<UInt32Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
        return CreateWellknownTypeValue<StringValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
        return CreateWellknownTypeValue<BytesValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
        return CreateWellknownTypeValue<BoolValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
        return CreateWellknownTypeValue<Any>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION:
        return CreateWellknownTypeValue<Duration>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP:
        return CreateWellknownTypeValue<Timestamp>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
        return CreateWellknownTypeValue<Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
        return CreateWellknownTypeValue<ListValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
        return CreateWellknownTypeValue<Struct>(message, factory, arena);
      // WELLKNOWNTYPE_FIELDMASK has no special CelValue type
      default:
        return absl::nullopt;
    }
  }

  // Non-copyable, non-assignable
  ValueFromMessageMaker(const ValueFromMessageMaker&) = delete;
  ValueFromMessageMaker& operator=(const ValueFromMessageMaker&) = delete;
};

CelValue DynamicList::operator[](int index) const {
  return ValueFactory(factory_, arena_)
      .ValueFromMessage(&values_->values(index));
}

absl::optional<CelValue> DynamicMap::operator[](CelValue key) const {
  CelValue::StringHolder str_key;
  if (!key.GetValue(&str_key)) {
    // Not a string key.
    return CreateErrorValue(arena_, absl::InvalidArgumentError(absl::StrCat(
                                        "Invalid map key type: '",
                                        CelValue::TypeName(key.type()), "'")));
  }

  auto it = values_->fields().find(std::string(str_key.value()));
  if (it == values_->fields().end()) {
    return absl::nullopt;
  }

  return ValueFactory(factory_, arena_).ValueFromMessage(&it->second);
}

google::protobuf::Message* MessageFromValue(const CelValue& value, Duration* duration) {
  absl::Duration val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  auto status = cel::internal::EncodeDuration(val, duration);
  if (!status.ok()) {
    return nullptr;
  }
  return duration;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, BoolValue* wrapper) {
  bool val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  wrapper->set_value(val);
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, BytesValue* wrapper) {
  CelValue::BytesHolder view_val;
  if (!value.GetValue(&view_val)) {
    return nullptr;
  }
  wrapper->set_value(view_val.value().data(), view_val.value().size());
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, DoubleValue* wrapper) {
  double val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  wrapper->set_value(val);
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, FloatValue* wrapper) {
  double val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  // Abort the conversion if the value is outside the float range.
  if (val > std::numeric_limits<float>::max()) {
    wrapper->set_value(std::numeric_limits<float>::infinity());
    return wrapper;
  }
  if (val < std::numeric_limits<float>::lowest()) {
    wrapper->set_value(-std::numeric_limits<float>::infinity());
    return wrapper;
  }
  wrapper->set_value(val);
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, Int32Value* wrapper) {
  int64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  // Abort the conversion if the value is outside the int32_t range.
  if (!cel::internal::CheckedInt64ToInt32(val).ok()) {
    return nullptr;
  }
  wrapper->set_value(val);
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, Int64Value* wrapper) {
  int64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  wrapper->set_value(val);
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, StringValue* wrapper) {
  CelValue::StringHolder view_val;
  if (!value.GetValue(&view_val)) {
    return nullptr;
  }
  wrapper->set_value(view_val.value().data(), view_val.value().size());
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, Timestamp* timestamp) {
  absl::Time val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  auto status = EncodeTime(val, timestamp);
  if (!status.ok()) {
    return nullptr;
  }
  return timestamp;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, UInt32Value* wrapper) {
  uint64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  // Abort the conversion if the value is outside the uint32_t range.
  if (!cel::internal::CheckedUint64ToUint32(val).ok()) {
    return nullptr;
  }
  wrapper->set_value(val);
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, UInt64Value* wrapper) {
  uint64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  wrapper->set_value(val);
  return wrapper;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, ListValue* json_list) {
  if (!value.IsList()) {
    return nullptr;
  }
  const CelList& list = *value.ListOrDie();
  for (int i = 0; i < list.size(); i++) {
    auto e = list[i];
    Value* elem = json_list->add_values();
    auto result = MessageFromValue(e, elem);
    if (result == nullptr) {
      return nullptr;
    }
  }
  return json_list;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, Struct* json_struct) {
  if (!value.IsMap()) {
    return nullptr;
  }
  const CelMap& map = *value.MapOrDie();
  const auto& keys = *map.ListKeys().value();
  auto fields = json_struct->mutable_fields();
  for (int i = 0; i < keys.size(); i++) {
    auto k = keys[i];
    // If the key is not a string type, abort the conversion.
    if (!k.IsString()) {
      return nullptr;
    }
    absl::string_view key = k.StringOrDie().value();

    auto v = map[k];
    if (!v.has_value()) {
      return nullptr;
    }
    Value field_value;
    auto result = MessageFromValue(*v, &field_value);
    // If the value is not a valid JSON type, abort the conversion.
    if (result == nullptr) {
      return nullptr;
    }
    (*fields)[std::string(key)] = field_value;
  }
  return json_struct;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, Value* json) {
  switch (value.type()) {
    case CelValue::Type::kBool: {
      bool val;
      if (value.GetValue(&val)) {
        json->set_bool_value(val);
        return json;
      }
    } break;
    case CelValue::Type::kBytes: {
      // Base64 encode byte strings to ensure they can safely be transpored
      // in a JSON string.
      CelValue::BytesHolder val;
      if (value.GetValue(&val)) {
        json->set_string_value(absl::Base64Escape(val.value()));
        return json;
      }
    } break;
    case CelValue::Type::kDouble: {
      double val;
      if (value.GetValue(&val)) {
        json->set_number_value(val);
        return json;
      }
    } break;
    case CelValue::Type::kDuration: {
      // Convert duration values to a protobuf JSON format.
      absl::Duration val;
      if (value.GetValue(&val)) {
        auto encode = cel::internal::EncodeDurationToString(val);
        if (!encode.ok()) {
          return nullptr;
        }
        json->set_string_value(*encode);
        return json;
      }
    } break;
    case CelValue::Type::kInt64: {
      int64_t val;
      // Convert int64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (value.GetValue(&val)) {
        if (IsJSONSafe(val)) {
          json->set_number_value(val);
        } else {
          json->set_string_value(absl::StrCat(val));
        }
        return json;
      }
    } break;
    case CelValue::Type::kString: {
      CelValue::StringHolder val;
      if (value.GetValue(&val)) {
        json->set_string_value(val.value().data(), val.value().size());
        return json;
      }
    } break;
    case CelValue::Type::kTimestamp: {
      // Convert timestamp values to a protobuf JSON format.
      absl::Time val;
      if (value.GetValue(&val)) {
        auto encode = cel::internal::EncodeTimeToString(val);
        if (!encode.ok()) {
          return nullptr;
        }
        json->set_string_value(*encode);
        return json;
      }
    } break;
    case CelValue::Type::kUint64: {
      uint64_t val;
      // Convert uint64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (value.GetValue(&val)) {
        if (IsJSONSafe(val)) {
          json->set_number_value(val);
        } else {
          json->set_string_value(absl::StrCat(val));
        }
        return json;
      }
    } break;
    case CelValue::Type::kList: {
      auto lv = MessageFromValue(value, json->mutable_list_value());
      if (lv != nullptr) {
        return json;
      }
    } break;
    case CelValue::Type::kMap: {
      auto sv = MessageFromValue(value, json->mutable_struct_value());
      if (sv != nullptr) {
        return json;
      }
    } break;
    case CelValue::Type::kNullType:
      json->set_null_value(protobuf::NULL_VALUE);
      return json;
    default:
      return nullptr;
  }
  return nullptr;
}

google::protobuf::Message* MessageFromValue(const CelValue& value, Any* any) {
  // In open source, any->PackFrom() returns void rather than boolean.
  switch (value.type()) {
    case CelValue::Type::kBool: {
      BoolValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kBytes: {
      BytesValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kDouble: {
      DoubleValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kDuration: {
      Duration v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kInt64: {
      Int64Value v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kString: {
      StringValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kTimestamp: {
      Timestamp v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kUint64: {
      UInt64Value v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kList: {
      ListValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kMap: {
      Struct v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kNullType: {
      Value v;
      auto msg = MessageFromValue(value, &v);
      if (msg != nullptr) {
        any->PackFrom(*msg);
        return any;
      }
    } break;
    case CelValue::Type::kMessage: {
      any->PackFrom(*(value.MessageOrDie()));
      return any;
    } break;
    default:
      break;
  }
  return nullptr;
}

// Factory class, responsible for populating a Message type instance with the
// value of a simple CelValue.
class MessageFromValueFactory {
 public:
  virtual ~MessageFromValueFactory() {}
  virtual const google::protobuf::Descriptor* GetDescriptor() const = 0;
  virtual absl::optional<const google::protobuf::Message*> WrapMessage(
      const CelValue& value, Arena* arena) const = 0;
};

// MessageFromValueMaker makes a specific protobuf Message instance based on
// the desired protobuf type name and an input CelValue.
//
// It holds a registry of CelValue factories for specific subtypes of Message.
// If message does not match any of types stored in registry, an the factory
// returns an absent value.
class MessageFromValueMaker {
 public:
  // Non-copyable, non-assignable
  MessageFromValueMaker(const MessageFromValueMaker&) = delete;
  MessageFromValueMaker& operator=(const MessageFromValueMaker&) = delete;

  template <class MessageType>
  static google::protobuf::Message* WrapWellknownTypeMessage(const CelValue& value,
                                                   Arena* arena) {
    // If the value is a message type, see if it is already of the proper type
    // name, and return it directly.
    if (value.IsMessage()) {
      const auto* msg = value.MessageOrDie();
      if (MessageType::descriptor()->well_known_type() ==
          msg->GetDescriptor()->well_known_type()) {
        return nullptr;
      }
    }
    // Otherwise, allocate an empty message type, and attempt to populate it
    // using the proper MessageFromValue overload.
    auto* msg_buffer = Arena::CreateMessage<MessageType>(arena);
    return MessageFromValue(value, msg_buffer);
  }

  static google::protobuf::Message* MaybeWrapMessage(const google::protobuf::Descriptor* descriptor,
                                           const CelValue& value,
                                           Arena* arena) {
    switch (descriptor->well_known_type()) {
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
        return WrapWellknownTypeMessage<DoubleValue>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
        return WrapWellknownTypeMessage<FloatValue>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
        return WrapWellknownTypeMessage<Int64Value>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
        return WrapWellknownTypeMessage<UInt64Value>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
        return WrapWellknownTypeMessage<Int32Value>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
        return WrapWellknownTypeMessage<UInt32Value>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
        return WrapWellknownTypeMessage<StringValue>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
        return WrapWellknownTypeMessage<BytesValue>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
        return WrapWellknownTypeMessage<BoolValue>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
        return WrapWellknownTypeMessage<Any>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION:
        return WrapWellknownTypeMessage<Duration>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP:
        return WrapWellknownTypeMessage<Timestamp>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
        return WrapWellknownTypeMessage<Value>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
        return WrapWellknownTypeMessage<ListValue>(value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
        return WrapWellknownTypeMessage<Struct>(value, arena);
      // WELLKNOWNTYPE_FIELDMASK has no special CelValue type
      default:
        return nullptr;
    }
  }
};

}  // namespace

CelValue UnwrapMessageToValue(const google::protobuf::Message* value,
                              const ProtobufValueFactory& factory,
                              Arena* arena) {
  // Messages are Nullable types
  if (value == nullptr) {
    return CelValue::CreateNull();
  }

  absl::optional<CelValue> special_value =
      ValueFromMessageMaker::CreateValue(value, factory, arena);
  if (special_value.has_value()) {
    return *special_value;
  }
  return factory(value);
}

const google::protobuf::Message* MaybeWrapValueToMessage(
    const google::protobuf::Descriptor* descriptor, const CelValue& value, Arena* arena) {
  google::protobuf::Message* msg =
      MessageFromValueMaker::MaybeWrapMessage(descriptor, value, arena);
  return msg;
}

}  // namespace google::api::expr::runtime::internal
