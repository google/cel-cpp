#include "eval/public/structs/cel_proto_wrapper.h"

#include <math.h>

#include <cstdint>
#include <limits>
#include <memory>

#include "google/protobuf/any.pb.h"
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
#include "common/overflow.h"
#include "eval/public/cel_value.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/proto_util.h"

namespace google::api::expr::runtime {

namespace {

using google::protobuf::Arena;
using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::Message;

using google::api::expr::internal::EncodeTime;
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

// kMaxIntJSON is defined as the Number.MAX_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMaxIntJSON = (1ll << 53) - 1;

// kMinIntJSON is defined as the Number.MIN_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMinIntJSON = -kMaxIntJSON;

// Forward declaration for google.protobuf.Value
CelValue ValueFromMessage(const Value* value, Arena* arena);
absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Value* json);

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
  DynamicList(const ListValue* values, Arena* arena)
      : arena_(arena), values_(values) {}

  CelValue operator[](int index) const override {
    return ValueFromMessage(&values_->values(index), arena_);
  }

  // List size
  int size() const override { return values_->values_size(); }

 private:
  Arena* arena_;
  const ListValue* values_;
};

// Map implementation wrapping google.protobuf.Struct.
class DynamicMap : public CelMap {
 public:
  DynamicMap(const Struct* values, Arena* arena)
      : arena_(arena), values_(values), key_list_(values) {}

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    CelValue::StringHolder str_key;
    if (!key.GetValue(&str_key)) {
      // Not a string key.
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", CelValue::TypeName(key.type()), "'"));
    }

    return values_->fields().contains(std::string(str_key.value()));
  }

  absl::optional<CelValue> operator[](CelValue key) const override {
    CelValue::StringHolder str_key;
    if (!key.GetValue(&str_key)) {
      // Not a string key.
      return CreateErrorValue(
          arena_,
          absl::InvalidArgumentError(absl::StrCat(
              "Invalid map key type: '", CelValue::TypeName(key.type()), "'")));
    }

    auto it = values_->fields().find(std::string(str_key.value()));
    if (it == values_->fields().end()) {
      return absl::nullopt;
    }

    return ValueFromMessage(&it->second, arena_);
  }

  int size() const override { return values_->fields_size(); }

  const CelList* ListKeys() const override { return &key_list_; }

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
  const Struct* values_;
  const DynamicMapKeyList key_list_;
};

// ValueFromMessage(....) function family.
// Functions of this family create CelValue object from specific subtypes of
// protobuf message.
CelValue ValueFromMessage(const Duration* duration, Arena*) {
  return CelProtoWrapper::CreateDuration(duration);
}

CelValue ValueFromMessage(const Timestamp* timestamp, Arena*) {
  return CelProtoWrapper::CreateTimestamp(timestamp);
}

CelValue ValueFromMessage(const ListValue* list_values, Arena* arena) {
  return CelValue::CreateList(
      Arena::Create<DynamicList>(arena, list_values, arena));
}

CelValue ValueFromMessage(const Struct* struct_value, Arena* arena) {
  return CelValue::CreateMap(
      Arena::Create<DynamicMap>(arena, struct_value, arena));
}

CelValue ValueFromMessage(const Any* any_value, Arena* arena) {
  auto type_url = any_value->type_url();
  auto pos = type_url.find_last_of('/');
  if (pos == absl::string_view::npos) {
    // TODO(issues/25) What error code?
    // Malformed type_url
    return CreateErrorValue(arena, "Malformed type_url string");
  }

  std::string full_name = std::string(type_url.substr(pos + 1));
  const Descriptor* nested_descriptor =
      DescriptorPool::generated_pool()->FindMessageTypeByName(full_name.data());

  if (nested_descriptor == nullptr) {
    // Descriptor not found for the type
    // TODO(issues/25) What error code?
    return CreateErrorValue(arena, "Descriptor not found");
  }

  const Message* prototype =
      google::protobuf::MessageFactory::generated_factory()->GetPrototype(
          nested_descriptor);
  if (prototype == nullptr) {
    // Failed to obtain prototype for the descriptor
    // TODO(issues/25) What error code?
    return CreateErrorValue(arena, "Prototype not found");
  }

  Message* nested_message = prototype->New(arena);
  if (!any_value->UnpackTo(nested_message)) {
    // Failed to unpack.
    // TODO(issues/25) What error code?
    return CreateErrorValue(arena, "Failed to unpack Any into message");
  }

  return CelProtoWrapper::CreateMessage(nested_message, arena);
}

CelValue ValueFromMessage(const BoolValue* wrapper, Arena*) {
  return CelValue::CreateBool(wrapper->value());
}

CelValue ValueFromMessage(const Int32Value* wrapper, Arena*) {
  return CelValue::CreateInt64(wrapper->value());
}

CelValue ValueFromMessage(const UInt32Value* wrapper, Arena*) {
  return CelValue::CreateUint64(wrapper->value());
}

CelValue ValueFromMessage(const Int64Value* wrapper, Arena*) {
  return CelValue::CreateInt64(wrapper->value());
}

CelValue ValueFromMessage(const UInt64Value* wrapper, Arena*) {
  return CelValue::CreateUint64(wrapper->value());
}

CelValue ValueFromMessage(const FloatValue* wrapper, Arena*) {
  return CelValue::CreateDouble(wrapper->value());
}

CelValue ValueFromMessage(const DoubleValue* wrapper, Arena*) {
  return CelValue::CreateDouble(wrapper->value());
}

CelValue ValueFromMessage(const StringValue* wrapper, Arena*) {
  return CelValue::CreateString(&wrapper->value());
}

CelValue ValueFromMessage(const BytesValue* wrapper, Arena* arena) {
  // BytesValue stores value as Cord
  return CelValue::CreateBytes(
      Arena::Create<std::string>(arena, std::string(wrapper->value())));
}

CelValue ValueFromMessage(const Value* value, Arena* arena) {
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
      return CelProtoWrapper::CreateMessage(&value->struct_value(), arena);
    case Value::KindCase::kListValue:
      return CelProtoWrapper::CreateMessage(&value->list_value(), arena);
    default:
      return CelValue::CreateNull();
  }
}

// Factory class, responsible for creating CelValue object from Message of some
// fixed subtype.
class ValueFromMessageFactory {
 public:
  virtual ~ValueFromMessageFactory() {}
  virtual const google::protobuf::Descriptor* GetDescriptor() const = 0;
  virtual absl::optional<CelValue> CreateValue(const google::protobuf::Message* value,
                                               Arena* arena) const = 0;
};

// This template class has a good performance, but performes downcast
// operations on google::protobuf::Message pointers.
template <class MessageType>
class CastingValueFromMessageFactory : public ValueFromMessageFactory {
 public:
  const google::protobuf::Descriptor* GetDescriptor() const override {
    return MessageType::descriptor();
  }

  absl::optional<CelValue> CreateValue(const google::protobuf::Message* msg,
                                       Arena* arena) const override {
    if (MessageType::descriptor() == msg->GetDescriptor()) {
      const MessageType* message =
          google::protobuf::DynamicCastToGenerated<const MessageType>(msg);
      if (message == nullptr) {
        auto message_copy = Arena::CreateMessage<MessageType>(arena);
        message_copy->CopyFrom(*msg);
        message = message_copy;
      }
      return ValueFromMessage(message, arena);
    }
    return absl::nullopt;
  }
};

// Class makes CelValue from generic protobuf Message.
// It holds a registry of CelValue factories for specific subtypes of Message.
// If message does not match any of types stored in registry, generic
// message-containing CelValue is created.
class ValueFromMessageMaker {
 public:
  explicit ValueFromMessageMaker() {
    Add(absl::make_unique<CastingValueFromMessageFactory<Duration>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<Timestamp>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<Value>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<Struct>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<ListValue>>());

    Add(absl::make_unique<CastingValueFromMessageFactory<Any>>());

    Add(absl::make_unique<CastingValueFromMessageFactory<BoolValue>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<Int32Value>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<UInt32Value>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<Int64Value>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<UInt64Value>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<FloatValue>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<DoubleValue>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<StringValue>>());
    Add(absl::make_unique<CastingValueFromMessageFactory<BytesValue>>());
  }

  absl::optional<CelValue> CreateValue(const google::protobuf::Message* value,
                                       Arena* arena) const {
    auto it = factories_.find(value->GetDescriptor());
    if (it == factories_.end()) {
      // Not found for value->GetDescriptor()->name()
      return absl::nullopt;
    }
    return (it->second)->CreateValue(value, arena);
  }

  // Non-copyable, non-assignable
  ValueFromMessageMaker(const ValueFromMessageMaker&) = delete;
  ValueFromMessageMaker& operator=(const ValueFromMessageMaker&) = delete;

 private:
  void Add(std::unique_ptr<ValueFromMessageFactory> factory) {
    const Descriptor* desc = factory->GetDescriptor();
    factories_.emplace(desc, std::move(factory));
  }

  absl::flat_hash_map<const google::protobuf::Descriptor*,
                      std::unique_ptr<ValueFromMessageFactory>>
      factories_;
};

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Duration* duration) {
  absl::Duration val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  auto status = google::api::expr::internal::EncodeDuration(val, duration);
  if (!status.ok()) {
    return absl::nullopt;
  }
  return duration;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        BoolValue* wrapper) {
  bool val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        BytesValue* wrapper) {
  CelValue::BytesHolder view_val;
  if (!value.GetValue(&view_val)) {
    return absl::nullopt;
  }
  wrapper->set_value(view_val.value().data());
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        DoubleValue* wrapper) {
  double val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        FloatValue* wrapper) {
  double val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
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

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Int32Value* wrapper) {
  int64_t val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  // Abort the conversion if the value is outside the int32_t range.
  if (!common::CheckedInt64ToInt32(val).ok()) {
    return absl::nullopt;
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Int64Value* wrapper) {
  int64_t val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        StringValue* wrapper) {
  CelValue::StringHolder view_val;
  if (!value.GetValue(&view_val)) {
    return absl::nullopt;
  }
  wrapper->set_value(view_val.value().data());
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Timestamp* timestamp) {
  absl::Time val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  auto status = EncodeTime(val, timestamp);
  if (!status.ok()) {
    return absl::nullopt;
  }
  return timestamp;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        UInt32Value* wrapper) {
  uint64_t val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  // Abort the conversion if the value is outside the uint32_t range.
  if (!common::CheckedUint64ToUint32(val).ok()) {
    return absl::nullopt;
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        UInt64Value* wrapper) {
  uint64_t val;
  if (!value.GetValue(&val)) {
    return absl::nullopt;
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        ListValue* json_list) {
  if (!value.IsList()) {
    return absl::nullopt;
  }
  const CelList& list = *value.ListOrDie();
  for (int i = 0; i < list.size(); i++) {
    auto e = list[i];
    Value* elem = json_list->add_values();
    auto result = MessageFromValue(e, elem);
    if (!result.has_value()) {
      return absl::nullopt;
    }
  }
  return json_list;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Struct* json_struct) {
  if (!value.IsMap()) {
    return absl::nullopt;
  }
  const CelMap& map = *value.MapOrDie();
  const auto& keys = *map.ListKeys();
  auto fields = json_struct->mutable_fields();
  for (int i = 0; i < keys.size(); i++) {
    auto k = keys[i];
    // If the key is not a string type, abort the conversion.
    if (!k.IsString()) {
      return absl::nullopt;
    }
    absl::string_view key = k.StringOrDie().value();

    auto v = map[k];
    if (!v.has_value()) {
      return absl::nullopt;
    }
    Value field_value;
    auto result = MessageFromValue(*v, &field_value);
    // If the value is not a valid JSON type, abort the conversion.
    if (!result.has_value()) {
      return absl::nullopt;
    }
    (*fields)[std::string(key)] = field_value;
  }
  return json_struct;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Value* json) {
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
        auto encode = google::api::expr::internal::EncodeDurationToString(val);
        if (!encode.ok()) {
          return absl::nullopt;
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
        json->set_string_value(val.value().data());
        return json;
      }
    } break;
    case CelValue::Type::kTimestamp: {
      // Convert timestamp values to a protobuf JSON format.
      absl::Time val;
      if (value.GetValue(&val)) {
        auto encode = google::api::expr::internal::EncodeTimeToString(val);
        if (!encode.ok()) {
          return absl::nullopt;
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
      if (lv.has_value()) {
        return json;
      }
    } break;
    case CelValue::Type::kMap: {
      auto sv = MessageFromValue(value, json->mutable_struct_value());
      if (sv.has_value()) {
        return json;
      }
    } break;
    default:
      if (value.IsNull()) {
        json->set_null_value(protobuf::NULL_VALUE);
        return json;
      }
      return absl::nullopt;
  }
  return absl::nullopt;
}

absl::optional<const google::protobuf::Message*> MessageFromValue(const CelValue& value,
                                                        Any* any) {
  // In open source, any->PackFrom() returns void rather than boolean.
  switch (value.type()) {
    case CelValue::Type::kBool: {
      BoolValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kBytes: {
      BytesValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kDouble: {
      DoubleValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kDuration: {
      Duration v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kInt64: {
      Int64Value v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kString: {
      StringValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kTimestamp: {
      Timestamp v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kUint64: {
      UInt64Value v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kList: {
      ListValue v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kMap: {
      Struct v;
      auto msg = MessageFromValue(value, &v);
      if (msg.has_value()) {
        any->PackFrom(**msg);
        return any;
      }
    } break;
    case CelValue::Type::kMessage: {
      if (value.IsNull()) {
        Value v;
        auto msg = MessageFromValue(value, &v);
        if (msg.has_value()) {
          any->PackFrom(**msg);
          return any;
        }
      } else {
        any->PackFrom(*(value.MessageOrDie()));
        return any;
      }
    } break;
    default:
      break;
  }
  return absl::nullopt;
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

// This template class has a good performance, but performes downcast
// operations on google::protobuf::Message pointers.
template <class MessageType>
class CastingMessageFromValueFactory : public MessageFromValueFactory {
 public:
  const google::protobuf::Descriptor* GetDescriptor() const override {
    return MessageType::descriptor();
  }

  absl::optional<const google::protobuf::Message*> WrapMessage(
      const CelValue& value, Arena* arena) const override {
    // Convert nulls separately from other messages as a null value is still
    // technically a message value, but not one that can be converted in the
    // standard way.
    if (value.IsNull()) {
      return MessageFromValue(value, Arena::CreateMessage<MessageType>(arena));
    }
    // If the value is a message type, see if it is already of the proper type
    // name, and return it directly.
    if (value.IsMessage()) {
      const auto* msg = value.MessageOrDie();
      if (MessageType::descriptor() == msg->GetDescriptor()) {
        return absl::nullopt;
      }
    }
    // Otherwise, allocate an empty message type, and attempt to populate it
    // using the proper MessageFromValue overload.
    auto* msg_buffer = Arena::CreateMessage<MessageType>(arena);
    return MessageFromValue(value, msg_buffer);
  }
};

// MessageFromValueMaker makes a specific protobuf Message instance based on
// the desired protobuf type name and an input CelValue.
//
// It holds a registry of CelValue factories for specific subtypes of Message.
// If message does not match any of types stored in registry, an the factory
// returns an absent value.
class MessageFromValueMaker {
 public:
  explicit MessageFromValueMaker() {
    Add(absl::make_unique<CastingMessageFromValueFactory<Any>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<BoolValue>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<BytesValue>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<DoubleValue>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<Duration>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<FloatValue>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<ListValue>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<Int32Value>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<Int64Value>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<StringValue>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<Struct>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<Timestamp>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<UInt32Value>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<UInt64Value>>());
    Add(absl::make_unique<CastingMessageFromValueFactory<Value>>());
  }
  // Non-copyable, non-assignable
  MessageFromValueMaker(const MessageFromValueMaker&) = delete;
  MessageFromValueMaker& operator=(const MessageFromValueMaker&) = delete;

  absl::optional<const google::protobuf::Message*> MaybeWrapMessage(
      absl::string_view type_name, const CelValue& value, Arena* arena) const {
    auto it = factories_.find(type_name);
    if (it == factories_.end()) {
      // Descriptor not found for type name.
      return absl::nullopt;
    }
    return (it->second)->WrapMessage(value, arena);
  }

 private:
  void Add(std::unique_ptr<MessageFromValueFactory> factory) {
    const Descriptor* desc = factory->GetDescriptor();
    factories_.emplace(desc->full_name(), std::move(factory));
  }

  absl::flat_hash_map<std::string, std::unique_ptr<MessageFromValueFactory>>
      factories_;
};

}  // namespace

// CreateMessage creates CelValue from google::protobuf::Message.
// As some of CEL basic types are subclassing google::protobuf::Message,
// this method contains type checking and downcasts.
CelValue CelProtoWrapper::CreateMessage(const google::protobuf::Message* value,
                                        Arena* arena) {
  static const ValueFromMessageMaker* maker = new ValueFromMessageMaker();

  // Messages are Nullable types
  if (value == nullptr) {
    return CelValue(value);
  }

  auto special_value = maker->CreateValue(value, arena);
  return special_value.has_value() ? special_value.value() : CelValue(value);
}

absl::optional<CelValue> CelProtoWrapper::MaybeWrapValue(
    absl::string_view type_name, const CelValue& value, Arena* arena) {
  static const MessageFromValueMaker* maker = new MessageFromValueMaker();

  auto msg = maker->MaybeWrapMessage(type_name, value, arena);
  if (!msg.has_value()) {
    return absl::nullopt;
  }
  return CelValue(msg.value());
}

}  // namespace google::api::expr::runtime
