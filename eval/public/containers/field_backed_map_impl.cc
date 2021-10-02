#include "eval/public/containers/field_backed_map_impl.h"

#include <limits>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/field_access.h"

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
  static bool LookupMapValue(const Reflection* reflection,
                             const Message& message,
                             const FieldDescriptor* field, const MapKey& key,
                             MapValueConstRef* val) {
    return reflection->LookupMapValue(message, field, key, val);
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
using google::protobuf::MapValueConstRef;
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
      return CreateErrorValue(arena_, status);
    }
    return key;
  }

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::Reflection* reflection_;
  google::protobuf::Arena* arena_;
};

bool MatchesMapKeyType(const FieldDescriptor* key_desc, const CelValue& key) {
  switch (key_desc->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return key.IsBool();
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return key.IsInt64();
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return key.IsUint64();
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return key.IsString();
    default:
      return false;
  }
}

absl::Status InvalidMapKeyType(absl::string_view key_type) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", key_type, "'"));
}

}  // namespace

FieldBackedMapImpl::FieldBackedMapImpl(
    const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* descriptor,
    google::protobuf::Arena* arena)
    : message_(message),
      descriptor_(descriptor),
      key_desc_(descriptor_->message_type()->FindFieldByNumber(kKeyTag)),
      value_desc_(descriptor_->message_type()->FindFieldByNumber(kValueTag)),
      reflection_(message_->GetReflection()),
      arena_(arena),
      key_list_(absl::make_unique<KeyList>(message, descriptor, arena)) {}

int FieldBackedMapImpl::size() const {
  return reflection_->FieldSize(*message_, descriptor_);
}

const CelList* FieldBackedMapImpl::ListKeys() const { return key_list_.get(); }

absl::StatusOr<bool> FieldBackedMapImpl::Has(const CelValue& key) const {
#ifdef GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
  MapValueConstRef value_ref;
  return LookupMapValue(key, &value_ref);
#else   // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
  return LegacyHasMapValue(key);
#endif  // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
}

absl::optional<CelValue> FieldBackedMapImpl::operator[](CelValue key) const {
#ifdef GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
  // Fast implementation which uses a friend method to do a hash-based key
  // lookup.
  MapValueConstRef value_ref;
  auto lookup_result = LookupMapValue(key, &value_ref);
  if (!lookup_result.ok()) {
    return CreateErrorValue(arena_, lookup_result.status());
  }
  if (!*lookup_result) {
    return absl::nullopt;
  }

  // Get value descriptor treating it as a repeated field.
  // All values in protobuf map have the same type.
  // The map is not empty, because LookupMapValue returned true.
  CelValue result = CelValue::CreateNull();
  const auto& status = CreateValueFromMapValue(message_, value_desc_,
                                               &value_ref, arena_, &result);
  if (!status.ok()) {
    return CreateErrorValue(arena_, status);
  }
  return result;

#else  // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
  // Default proto implementation, does not use fast-path key lookup.
  return LegacyLookupMapValue(key);
#endif  // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
}

absl::StatusOr<bool> FieldBackedMapImpl::LookupMapValue(
    const CelValue& key, MapValueConstRef* value_ref) const {
#ifdef GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND

  if (!MatchesMapKeyType(key_desc_, key)) {
    return InvalidMapKeyType(key_desc_->cpp_type_name());
  }

  google::protobuf::MapKey proto_key;
  switch (key_desc_->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
      bool key_value;
      key.GetValue(&key_value);
      proto_key.SetBoolValue(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
      int64_t key_value;
      key.GetValue(&key_value);
      if (key_value > std::numeric_limits<int32_t>::max() ||
          key_value < std::numeric_limits<int32_t>::lowest()) {
        return absl::OutOfRangeError("integer overflow");
      }
      proto_key.SetInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
      int64_t key_value;
      key.GetValue(&key_value);
      proto_key.SetInt64Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      CelValue::StringHolder key_value;
      key.GetValue(&key_value);
      auto str = key_value.value();
      proto_key.SetStringValue(std::string(str.begin(), str.end()));
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
      uint64_t key_value;
      key.GetValue(&key_value);
      if (key_value > std::numeric_limits<uint32_t>::max()) {
        return absl::OutOfRangeError("unsigned integer overlow");
      }
      proto_key.SetUInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
      uint64_t key_value;
      key.GetValue(&key_value);
      proto_key.SetUInt64Value(key_value);
    } break;
    default:
      return InvalidMapKeyType(key_desc_->cpp_type_name());
  }
  // Look the value up
  return google::protobuf::expr::CelMapReflectionFriend::LookupMapValue(
      reflection_, *message_, descriptor_, proto_key, value_ref);
#else   // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
  return absl::UnimplementedError("fast-path key lookup not implemented");
#endif  // GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
}

absl::StatusOr<bool> FieldBackedMapImpl::LegacyHasMapValue(
    const CelValue& key) const {
  auto lookup_result = LegacyLookupMapValue(key);
  if (!lookup_result.has_value()) {
    return false;
  }
  auto result = *lookup_result;
  if (result.IsError()) {
    return *(result.ErrorOrDie());
  }
  return true;
}

absl::optional<CelValue> FieldBackedMapImpl::LegacyLookupMapValue(
    const CelValue& key) const {
  // Ensure that the key matches the key type.
  if (!MatchesMapKeyType(key_desc_, key)) {
    return CreateErrorValue(arena_,
                            InvalidMapKeyType(key_desc_->cpp_type_name()));
  }

  CelValue proto_key = CelValue::CreateNull();
  int map_size = size();
  for (int i = 0; i < map_size; i++) {
    const Message* entry =
        &reflection_->GetRepeatedMessage(*message_, descriptor_, i);
    if (entry == nullptr) continue;

    // Key Tag == 1
    auto status =
        CreateValueFromSingleField(entry, key_desc_, arena_, &proto_key);
    if (!status.ok()) {
      return CreateErrorValue(arena_, status);
    }

    bool match = false;
    switch (key_desc_->cpp_type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        match = key.BoolOrDie() == proto_key.BoolOrDie();
        break;
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        // fall through
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        match = key.Int64OrDie() == proto_key.Int64OrDie();
        break;
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        // fall through
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        match = key.Uint64OrDie() == proto_key.Uint64OrDie();
        break;
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        match = key.StringOrDie() == proto_key.StringOrDie();
        break;
      default:
        // this would normally indicate a bad key type, which should not be
        // possible based on the earlier test.
        break;
    }

    if (match) {
      CelValue result = CelValue::CreateNull();
      auto status =
          CreateValueFromSingleField(entry, value_desc_, arena_, &result);
      if (!status.ok()) {
        return CreateErrorValue(arena_, status);
      }
      return result;
    }
  }
  return {};
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
