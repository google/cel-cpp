#include "eval/eval/field_backed_map_impl.h"
#include "google/protobuf/map_field.h"
#include "eval/eval/field_access.h"
#include "eval/public/cel_value.h"

#ifdef GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND

namespace google {
namespace protobuf {
namespace expr {

// CelMapReflectionFriend provides access to Reflection's private methods. The
// class is a friend of google::protobuf::Reflection. We do not add FieldBackedMapImpl as
// a friend directly, because it belongs to google:: namespace. The build of
// protobuf fails on MSVC if this namespace is used, probably because
// of macros usage.
class CelMapReflectionFriend {
 public:
  static bool ContainsMapKey(const Reflection* reflection,
                             const Message& message,
                             const FieldDescriptor* field, const MapKey& key) {
    return reflection->ContainsMapKey(message, field, key);
  }

  static bool InsertOrLookupMapValue(const Reflection* reflection,
                                     Message* message,
                                     const FieldDescriptor* field,
                                     const MapKey& key, MapValueRef* val) {
    return reflection->InsertOrLookupMapValue(message, field, key, val);
  }
};

}  // namespace expr
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {
using google::protobuf::Arena;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::MapValueRef;
using google::protobuf::Message;

// Map entries have two field tags
// 1 - for key
// 2 - for value
constexpr int kKeyTag = 1;
constexpr int kValueTag = 2;

class KeyList : public CelList {
 public:
  // message contains the "repeated" field
  // descriptor FieldDescriptor for the field
  KeyList(const google::protobuf::Message* message,
          const google::protobuf::FieldDescriptor* descriptor, google::protobuf::Arena* arena)
      : message_(message),
        descriptor_(descriptor),
        reflection_(message_->GetReflection()),
        arena_(arena) {}

  // List size.
  int size() const override {
    return reflection_->FieldSize(*message_, descriptor_);
  }

  // List element access operator.
  CelValue operator[](int index) const override {
    CelValue key = CelValue::CreateNull();
    const Message* entry =
        &reflection_->GetRepeatedMessage(*message_, descriptor_, index);

    if (entry == nullptr) {
      return CelValue::CreateNull();
    }

    const Descriptor* entry_descriptor = entry->GetDescriptor();
    // Key Tag == 1
    const FieldDescriptor* key_desc =
        entry_descriptor->FindFieldByNumber(kKeyTag);

    auto status = CreateValueFromSingleField(entry, key_desc, arena_, &key);
    if (!status.ok()) {
      return CreateErrorValue(arena_, status.message());
    }
    return key;
  }

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::Reflection* reflection_;
  google::protobuf::Arena* arena_;
};

}  // namespace

FieldBackedMapImpl::FieldBackedMapImpl(
    const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* descriptor,
    google::protobuf::Arena* arena)
    : message_(message),
      descriptor_(descriptor),
      reflection_(message_->GetReflection()),
      arena_(arena),
      key_list_(absl::make_unique<KeyList>(message, descriptor, arena)) {}

int FieldBackedMapImpl::size() const {
  return reflection_->FieldSize(*message_, descriptor_);
}

const CelList* FieldBackedMapImpl::ListKeys() const { return key_list_.get(); }

absl::optional<CelValue> FieldBackedMapImpl::operator[](CelValue key) const {
#ifdef GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
  // Fast implementation.
  google::protobuf::MapKey inner_key;
  switch (key.type()) {
    case CelValue::Type::kInt64: {
      inner_key.SetInt64Value(key.Int64OrDie());
      break;
    }
    case CelValue::Type::kUint64: {
      inner_key.SetUInt64Value(key.Uint64OrDie());
      break;
    }
    case CelValue::Type::kString: {
      auto str = key.StringOrDie().value();
      inner_key.SetStringValue(std::string(str.begin(), str.end()));
      break;
    }
    default: { return {}; }
  }
  // Performance issue. Currently the only way to do a lookup is
  // InsertOrLookupMapValue. This function will modify the map if the key
  // doesn't exist, that is why we have to call ContainsMapKey first, which
  // results in hashing the key more than once.
  if (!google::protobuf::expr::CelMapReflectionFriend::ContainsMapKey(
          reflection_, *message_, descriptor_, inner_key)) {
    return {};
  }
  MapValueRef value_ref;
  // InsertOrLookupMapValue is not marked as const (but it is const in this
  // scenario when ContainsMapKey returns true), so we use const_cast.
  if (google::protobuf::expr::CelMapReflectionFriend::InsertOrLookupMapValue(
          reflection_, const_cast<google::protobuf::Message*>(message_), descriptor_,
          inner_key, &value_ref)) {
    GOOGLE_LOG(ERROR) << "The map was expected to have the key, but it didn't.";
  }
  // Get value descriptor treating it as a repeated field.
  // All values in protobuf map have the same type.
  // The map is not empty, because ContainsMapKey returned true.
  const Message* entry =
      &reflection_->GetRepeatedMessage(*message_, descriptor_, 0);
  if (entry == nullptr) {
    return {};
  }
  const Descriptor* entry_descriptor = entry->GetDescriptor();
  const FieldDescriptor* value_desc =
      entry_descriptor->FindFieldByNumber(kValueTag);

  CelValue result = CelValue::CreateNull();
  auto status = CreateValueFromMapValue(message_, value_desc, &value_ref,
                                        arena_, &result);
  if (!status.ok()) {
    return CreateErrorValue(arena_, status.message());
  }
  return result;
#else   // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
  // Slow implementation.
  CelValue result = CelValue::CreateNull();
  CelValue inner_key = CelValue::CreateNull();

  int map_size = size();
  for (int i = 0; i < map_size; i++) {
    const Message* entry =
        &reflection_->GetRepeatedMessage(*message_, descriptor_, i);

    if (entry == nullptr) continue;

    const Descriptor* entry_descriptor = entry->GetDescriptor();
    // Key Tag == 1
    const FieldDescriptor* key_desc =
        entry_descriptor->FindFieldByNumber(kKeyTag);

    auto status =
        CreateValueFromSingleField(entry, key_desc, arena_, &inner_key);
    if (!status.ok()) {
      return CreateErrorValue(arena_, status.ToString());
    }

    if (key.type() != inner_key.type()) {
      continue;
    }

    bool match = false;
    switch (key.type()) {
      case CelValue::Type::kInt64:
        match = key.Int64OrDie() == inner_key.Int64OrDie();
        break;
      case CelValue::Type::kUint64:
        match = key.Uint64OrDie() == inner_key.Uint64OrDie();
        break;
      case CelValue::Type::kString:
        match = key.StringOrDie() == inner_key.StringOrDie();
        break;
      default:
        match = false;
    }

    if (match) {
      const FieldDescriptor* value_desc =
          entry_descriptor->FindFieldByNumber(kValueTag);

      auto status =
          CreateValueFromSingleField(entry, value_desc, arena_, &result);
      if (!status.ok()) {
        return CreateErrorValue(arena_, status.message());
      }

      return result;
    }
  }

  return {};
#endif  // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
