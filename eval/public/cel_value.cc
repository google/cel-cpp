#include "eval/public/cel_value.h"

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/container/node_hash_map.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "internal/proto_util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::protobuf::Arena;
using google::protobuf::Message;
using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;

using google::protobuf::Any;
using google::protobuf::Duration;
using google::protobuf::Timestamp;
using google::protobuf::Value;
using google::protobuf::Struct;
using google::protobuf::ListValue;
using google::protobuf::BoolValue;
using google::protobuf::BytesValue;
using google::protobuf::DoubleValue;
using google::protobuf::FloatValue;
using google::protobuf::Int32Value;
using google::protobuf::Int64Value;
using google::protobuf::StringValue;
using google::protobuf::UInt32Value;
using google::protobuf::UInt64Value;

constexpr char kErrNoMatchingOverload[] = "No matching overloads found";
constexpr char kErrNoSuchKey[] = "Key not found in map";

// Forward declaration for google.protobuf.Value
CelValue ValueFromMessage(const Value* value, Arena* arena);

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

  absl::optional<CelValue> operator[](CelValue key) const override {
    CelValue::StringHolder str_key;
    if (!key.GetValue(&str_key)) {
      return {};  // Not a string key
    }

    auto it = values_->fields().find(std::string(str_key.value()));
    if (it == values_->fields().end()) {
      return {};
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
  return CelValue::CreateDuration(duration);
}

CelValue ValueFromMessage(const Timestamp* timestamp, Arena*) {
  return CelValue::CreateTimestamp(timestamp);
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

  auto pos = type_url.find_last_of("/");
  if (pos == absl::string_view::npos) {
    // TODO(issues/25) What error code?
    // Malformed type_url
    return CreateErrorValue(arena, "Malformed type_url string");
  }

  std::string full_name = std::string(type_url.substr(pos + 1));
  const Descriptor* nested_descriptor =
      DescriptorPool::generated_pool()->FindMessageTypeByName(full_name);

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

  return CelValue::CreateMessage(nested_message, arena);
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
      Arena::Create<std::string>(arena, wrapper->value()));
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
      return CelValue::CreateMessage(&value->struct_value(), arena);
    case Value::KindCase::kListValue:
      return CelValue::CreateMessage(&value->list_value(), arena);
    default:
      return CreateErrorValue(arena, "No known fields set in Value message");
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
      return ValueFromMessage(
          google::protobuf::DynamicCastToGenerated<const MessageType>(msg), arena);
    }
    return {};
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
      return {};
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

  absl::node_hash_map<const google::protobuf::Descriptor*,
                      std::unique_ptr<ValueFromMessageFactory>>
      factories_;
};

}  // namespace

// CreateMessage creates CelValue from google::protobuf::Message.
// As some of CEL basic types are subclassing google::protobuf::Message,
// this method contains type checking and downcasts.
CelValue CelValue::CreateMessage(const google::protobuf::Message* value, Arena* arena) {
  static const ValueFromMessageMaker* maker = new ValueFromMessageMaker();

  // Messages are Nullable types
  if (value == nullptr) {
    return CelValue(value);
  }

  auto special_value = maker->CreateValue(value, arena);

  return special_value.has_value() ? special_value.value() : CelValue(value);
}

std::string CelValue::TypeName(Type value_type) {
  switch (value_type) {
    case Type::kBool:
      return "bool";
    case Type::kInt64:
      return "int64";
    case Type::kUint64:
      return "uint64";
    case Type::kDouble:
      return "double";
    case Type::kString:
      return "string";
    case Type::kBytes:
      return "bytes";
    case Type::kMessage:
      return "Message";
    case Type::kDuration:
      return "Duration";
    case Type::kTimestamp:
      return "Timestamp";
    case Type::kList:
      return "CelList";
    case Type::kMap:
      return "CelMap";
    case Type::kError:
      return "CelError";
    default:
      return "UnknownType";
  }
}

CelValue CreateErrorValue(Arena* arena, absl::string_view message,
                          cel_base::StatusCode error_code, int) {
  CelError* error = Arena::Create<CelError>(arena, error_code, message);
  return CelValue::CreateError(error);
}

CelValue CreateNoMatchingOverloadError(google::protobuf::Arena* arena) {
  return CreateErrorValue(arena, kErrNoMatchingOverload,
                          cel_base::StatusCode::kUnknown);
}

bool CheckNoMatchingOverloadError(CelValue value) {
  return value.IsError() &&
         value.ErrorOrDie()->message() == kErrNoMatchingOverload;
}

CelValue CreateNoSuchFieldError(google::protobuf::Arena* arena) {
  return CreateErrorValue(arena, "no_such_field", cel_base::StatusCode::kNotFound);
}

CelValue CreateNoSuchKeyError(google::protobuf::Arena* arena, absl::string_view) {
  return CreateErrorValue(arena, kErrNoSuchKey, cel_base::StatusCode::kNotFound);
}

bool CheckNoSuchKeyError(CelValue value) {
  return value.IsError() && value.ErrorOrDie()->message() == kErrNoSuchKey;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
