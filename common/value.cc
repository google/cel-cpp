#include "common/value.h"

#include <cmath>
#include <functional>
#include <type_traits>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/utility/utility.h"
#include "common/macros.h"
#include "internal/cel_printer.h"
#include "internal/hash_util.h"
#include "internal/status_util.h"

namespace google {
namespace api {
namespace expr {
namespace common {

namespace {

static constexpr const Value::Kind kIndexToKind[] = {
    Value::Kind::kNull,      // null value
    Value::Kind::kBool,      // bool value
    Value::Kind::kInt,       // int value
    Value::Kind::kUInt,      // uint value
    Value::Kind::kDouble,    // double value
    Value::Kind::kEnum,      // enum_value
    Value::Kind::kType,      // type value
    Value::Kind::kType,      // type value
    Value::Kind::kType,      // type value
    Value::Kind::kUnknown,   // unknown
    Value::Kind::kString,    // string value
    Value::Kind::kString,    // string value
    Value::Kind::kString,    // string value
    Value::Kind::kBytes,     // bytes value
    Value::Kind::kBytes,     // bytes value
    Value::Kind::kBytes,     // bytes value
    Value::Kind::kMap,       // map value
    Value::Kind::kMap,       // map value
    Value::Kind::kList,      // list value
    Value::Kind::kList,      // list value
    Value::Kind::kObject,    // object value
    Value::Kind::kObject,    // object value
    Value::Kind::kDuration,  // duration
    Value::Kind::kTime,      // time
    Value::Kind::kEnum,      // enum_value
    Value::Kind::kType,      // type value
    Value::Kind::kError,     // error
    Value::Kind::kUnknown,   // unknown
};

}  // namespace

Value::~Value() {
  // All the enums should line up.
  static_assert(28 == absl::variant_size<ValueData>::value, "size mismatch");
  static_assert(DATA_SIZE == absl::variant_size<ValueData>::value,
                "size mismatch");
  static_assert(
      ABSL_ARRAYSIZE(kIndexToKind) == absl::variant_size<ValueData>::value,
      "size mismatch");

  static_assert(static_cast<std::size_t>(Kind::DO_NOT_USE) ==
                    internal::types_size<KindToType>::value,
                "size mismatch");

  // Value size should not exceed the size of a basic variant type.
  static_assert(sizeof(Value) <= sizeof(absl::variant<int64_t, std::nullptr_t>),
                "Value oversized");
}

Value::Kind Value::kind() const { return kIndexToKind[data_.index()]; }

bool Value::owns_value() const {
  if (is_inline() || data_.index() >= kOptionalOwnershipEnd) {
    return true;
  }

  switch (data_.index()) {
    case kBytes:
    case kStr:
      return true;
    case kList:
      return absl::get<kList>(data_)->owns_value();
    case kMap:
      return absl::get<kMap>(data_)->owns_value();
    case kObject:
      return absl::get<kObject>(data_)->owns_value();
    default:
      return false;
  }
}

Value Value::GetType() const {
  switch (kind()) {
    // Basic values
    case Kind::kNull:
      return Value::FromType(BasicTypeValue::kNull);
    case Kind::kBool:
      return Value::FromType(BasicTypeValue::kBool);
    case Kind::kInt:
      return Value::FromType(BasicTypeValue::kInt);
    case Kind::kUInt:
      return Value::FromType(BasicTypeValue::kUint);
    case Kind::kDouble:
      return Value::FromType(BasicTypeValue::kDouble);
    case Kind::kString:
      return Value::FromType(BasicTypeValue::kString);
    case Kind::kBytes:
      return Value::FromType(BasicTypeValue::kBytes);
    case Kind::kType:
      return Value::FromType(BasicTypeValue::kType);
    case Kind::kMap:
      return Value::FromType(BasicTypeValue::kMap);
    case Kind::kList:
      return Value::FromType(BasicTypeValue::kList);

      // Enum
    case Kind::kEnum:
      return Value::FromType(enum_value().type());

      // Objects.
    case Kind::kObject:
      return Value::FromType(object_value().object_type());
    case Kind::kDuration:
      return Value::FromType(
          ObjectType(google::protobuf::Duration::descriptor()));
    case Kind::kTime:
      return Value::FromType(
          ObjectType(google::protobuf::Timestamp::descriptor()));

    // Non-values.
    case Kind::kError:
    case Kind::kUnknown:
      return *this;

    // Cause a compiler error if this switch isn't complete.
    case Kind::DO_NOT_USE:
      assert(false);
  }
  // Should never happen.
  return Value::FromError(
      internal::InternalError(absl::StrCat("Bad value kind: ", kind())));
}

bool Value::operator==(const Value& rhs) const {
  return kind() == rhs.kind() &&
         Value::visit(internal::StrictEqVisitor(), *this, rhs);
}

std::size_t Value::hash_code() const {
  return internal::MixHash(internal::Hash(kind()), visit(internal::Hasher()));
}

std::string Value::ToString() const {
  switch (data_.index()) {
    case kId:
      return unknown_value().ToString();
    case kBytes:
    case kBytesView:
    case kBytesPtr:
      return absl::StrCat("b", internal::ToString(bytes_value()));
  }
  return visit(internal::CelPrinter());
}

Value Value::ForString(absl::string_view value,
                       const absl::optional<RefProvider>& parent) {
  if (parent == absl::nullopt) {
    return Value::FromString(value);
  } else if (!parent->RequiresReference()) {
    return Create<kStrPtr>(value);
  }
  return Create<kStrView>(parent->GetRef(), value);
}

Value Value::ForBytes(absl::string_view value, const ParentRef& parent) {
  if (parent == absl::nullopt) {
    return FromBytes(value);
  } else if (parent->RequiresReference()) {
    return Create<kBytesView>(parent->GetRef(), value);
  }
  return Create<kBytesPtr>(value);
}

Value Value::FromEnum(const EnumValue& value) {
  if (value.is_named()) {
    return FromEnum(value.named_value());
  }
  return FromEnum(value.unnamed_value());
}

Value Value::FromType(const Type& value) {
  if (value.is_basic()) {
    return FromType(value.basic_type());
  }
  if (value.is_enum()) {
    return FromType(value.enum_type());
  }
  if (value.is_object()) {
    return FromType(value.object_type());
  }
  return Create<kUnrecognizedType>(value.full_name());
}

Value Value::FromUnknown(const Unknown& value) {
  if (value.ids().size() == 1) {
    // Only has a single id, so it can be stored inline.
    return FromUnknown(*value.ids().begin());
  }
  return Create<kUnknown>(value);
}

Value Value::FromUnknown(Unknown&& value) {
  if (value.ids().size() == 1) {
    return FromUnknown(*value.ids().begin());
  }
  return Create<kUnknown>(std::move(value));
}

std::size_t Container::hash_code() const {
  return internal::LazyComputeHash([this]() { return ComputeHash(); },
                                   &hash_code_);
}

Container::Container() : hash_code_(internal::kNoHash) {}

Value Container::GetToContainsResult(const Value& get_result) {
  if (get_result.kind() == Value::Kind::kUnknown) {
    return get_result;
  }
  return Value::FromBool(get_result.is_value());
}

Value List::Get(const Value& value) const {
  RETURN_IF_NOT_VALUE(value);
  auto index = value.get_if<std::size_t>();
  if (!index) {
    return Value::FromError(
        internal::UnexpectedType(value.GetType().ToString(), "list index"));
  }
  return Get(*index);
}

Value List::Contains(const Value& value) const {
  RETURN_IF_NOT_VALUE(value);
  return ContainsImpl(value);
}

google::rpc::Status List::ForEach(
    const std::function<google::rpc::Status(const Value&)>& call) const {
  for (std::size_t i = 0; i < size(); ++i) {
    RETURN_IF_STATUS_ERROR(call(Get(i)));
  }
  return internal::OkStatus();
}

Value List::ContainsImpl(const Value& value) const {
  return Value::FromBool(
      !ForEach([&value](const Value& elem) { return value != elem; }));
}

bool List::operator==(const List& rhs) const {
  if (this == &rhs) {
    return true;
  }
  if (size() != rhs.size() || hash_code() != rhs.hash_code()) {
    return false;
  }
  for (std::size_t i = 0; i < size(); ++i) {
    if (Get(i) != rhs.Get(i)) {
      return false;
    }
  }
  return true;
}

std::size_t List::ComputeHash() const {
  std::size_t code = 0;
  ForEach(
      [&code](const Value& value) { internal::AccumulateHash(value, &code); });
  return code;
}

std::string List::ToString() const {
  internal::SequenceBuilder<internal::ListJoinPolicy> builder;
  ForEach([&builder](const Value& elem) { builder.Add(elem); });
  return builder.Build();
}

Value Map::Get(const Value& key) const {
  RETURN_IF_NOT_VALUE(key);
  return GetImpl(key);
}

Value Map::ContainsKey(const Value& key) const {
  RETURN_IF_NOT_VALUE(key);
  return ContainsKeyImpl(key);
}

Value Map::ContainsKeyImpl(const Value& key) const {
  return GetToContainsResult(GetImpl(key));
}

Value Map::ContainsValue(const Value& value) const {
  RETURN_IF_NOT_VALUE(value);
  return ContainsValueImpl(value);
}

Value Map::ContainsValueImpl(const Value& value) const {
  return Value::FromBool(
      !ForEach([&value](const Value& key, const Value& stored_value) {
        return stored_value != value;
      }));
}

Value Map::GetImpl(const Value& key) const {
  Value value;
  bool found = !ForEach(
      [&key, &value](const Value& stored_key, const Value& stored_value) {
        if (stored_key == key) {
          value = stored_value;
          return false;
        }
        return true;
      });
  return found ? value : Value::FromError(internal::NoSuchKey(key.ToString()));
}

std::size_t Map::ComputeHash() const {
  std::size_t code = 0;
  ForEach([&code](const Value& key, const Value& value) {
    internal::AccumulateHashNoOrder(internal::Hash(key, value), &code);
  });
  return code;
}

bool Map::operator==(const Map& rhs) const {
  if (this == &rhs) {
    return true;
  }
  if (size() != rhs.size() || hash_code() != rhs.hash_code()) {
    return false;
  }
  return rhs.ForEach([this](const Value& key, const Value& value) {
    return Get(key) == value;
  });
}

std::string Map::ToString() const {
  internal::SequenceBuilder<internal::MapJoinPolicy> builder;
  ForEach([&builder](const Value& key, const Value& value) {
    builder.Add(key, value);
  });
  return builder.Build();
}

Value Object::ContainsMember(absl::string_view name) const {
  return GetToContainsResult(GetMember(name));
}

std::string Object::ToString() const {
  internal::SequenceBuilder<internal::ObjectJoinPolicy> builder;
  ForEach([&builder](absl::string_view name, Value value) {
    builder.Add(name, value);
  });
  return builder.Build(object_type().full_name());
}

bool Object::operator==(const Object& rhs) const {
  if (this == &rhs) {
    return true;
  }
  if (object_type() != rhs.object_type() || hash_code() != rhs.hash_code()) {
    return false;
  }
  return EqualsImpl(rhs);
}

std::size_t Object::ComputeHash() const {
  std::size_t hash_code = 0;
  ForEach([&hash_code](absl::string_view key, const Value& value) {
    internal::AccumulateHashNoOrder(internal::Hash(key, value), &hash_code);
  });
  return hash_code;
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
