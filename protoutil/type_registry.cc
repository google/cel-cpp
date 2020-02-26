#include "protoutil/type_registry.h"

#include "google/protobuf/reflection.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/container/node_hash_map.h"
#include "common/macros.h"
#include "internal/map_impl.h"
#include "internal/proto_util.h"

namespace google {
namespace api {
namespace expr {
namespace protoutil {
namespace {

common::Value MissingCall(const common::ObjectType& type) {
  return common::Value::FromError(internal::InternalError(
      absl::StrCat("Missing callback for ", type.value()->full_name())));
}

absl::string_view FindObjectType(const google::protobuf::Any* value) {
  absl::string_view object_type = value->type_url();
  // Unfortunately the proto2 function that does this is internal.
  return object_type.substr(object_type.find('/') + 1);
}

google::protobuf::util::MessageDifferencer& GetDiffer() {
  static google::protobuf::util::MessageDifferencer* differ = []() {
    auto* comp = new google::protobuf::util::DefaultFieldComparator();
    comp->set_float_comparison(google::protobuf::util::DefaultFieldComparator::EXACT);
    comp->set_treat_nan_as_equal(true);

    auto* differ = new google::protobuf::util::MessageDifferencer;
    differ->set_field_comparator(comp);
    return differ;
  }();
  return *differ;
}

template <typename T>
class BaseProtoList : public common::List {
 public:
  explicit BaseProtoList(const common::ParentRef& parent,
                         const google::protobuf::RepeatedFieldRef<T>& value)
      : parent_ref_(parent->GetRef()), value_(value) {}

  inline std::size_t size() const final { return value_.size(); }
  inline bool owns_value() const final { return true; }

 protected:
  common::ValueRef parent_ref_;

  google::protobuf::RepeatedFieldRef<T> value_;
};

template <typename T, common::Value::Kind ValueKind>
class ProtoList final : public BaseProtoList<T> {
 public:
  explicit ProtoList(const common::ParentRef& parent,
                     const google::protobuf::RepeatedFieldRef<T>& value)
      : BaseProtoList<T>(parent, value) {}

  common::Value Get(std::size_t index) const override {
    return common::List::GetValue<ValueKind>(this->value_.Get(index));
  }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const common::Value&)>& call)
      const override {
    for (const auto& elem : this->value_) {
      RETURN_IF_STATUS_ERROR(call(common::List::GetValue<ValueKind>(elem)));
    }
    return internal::OkStatus();
  }
};

class BaseProtoRefList : public common::List {
 public:
  BaseProtoRefList(const common::ParentRef& parent, const google::protobuf::Message* msg,
                   const google::protobuf::FieldDescriptor* field)
      : parent_ref_(parent->GetRef()), msg_(msg), field_(field) {}

  inline std::size_t size() const final {
    return msg_->GetReflection()->FieldSize(*msg_, field_);
  }
  inline bool owns_value() const final { return true; }

 protected:
  common::ValueRef parent_ref_;
  const google::protobuf::Message* msg_;
  const google::protobuf::FieldDescriptor* field_;
};

template <common::Value::Kind ValueKind>
class ProtoStrList final : public BaseProtoRefList {
 public:
  ProtoStrList(const common::ParentRef& parent, const google::protobuf::Message* msg,
               const google::protobuf::FieldDescriptor* field)
      : BaseProtoRefList(parent, msg, field) {}

  common::Value Get(std::size_t index) const override {
    std::string scratch;
    const std::string& value =
        msg_->GetReflection()->GetRepeatedStringReference(*msg_, field_, index,
                                                          &scratch);
    if (&value == &scratch) {
      return common::Value::From<ValueKind>(value);
    }
    return common::Value::For<ValueKind>(value, SelfRefProvider());
  }
};

class ProtoMsgList final : public BaseProtoRefList {
 public:
  ProtoMsgList(const TypeRegistry* reg, const common::ParentRef& parent,
               const google::protobuf::Message* msg, const google::protobuf::FieldDescriptor* field)
      : BaseProtoRefList(parent, msg, field), reg_(reg) {}

  common::Value Get(std::size_t index) const override {
    return reg_->ValueFor(
        &msg_->GetReflection()->GetRepeatedMessage(*msg_, field_, index),
        SelfRefProvider());
  }

 private:
  const TypeRegistry* reg_;
};

class ProtoEnumList final : public BaseProtoRefList {
 public:
  ProtoEnumList(const TypeRegistry* reg, const common::ParentRef& parent,
                const google::protobuf::Message* msg,
                const google::protobuf::FieldDescriptor* field)
      : BaseProtoRefList(parent, msg, field), reg_(reg) {}

  common::Value Get(std::size_t index) const override {
    return reg_->ValueFrom(
        common::EnumType(field_->enum_type()),
        msg_->GetReflection()->GetRepeatedEnumValue(*msg_, field_, index));
  }

 private:
  const TypeRegistry* reg_;
};

/**
 * A Object for a google.protobuf.Any that could not be decoded.
 */
template <typename HolderPolicy>
class UnrecognizedMessageObject final : public common::Object {
 public:
  template <typename T>
  UnrecognizedMessageObject(T&& value) : holder_(std::forward<T>(value)) {}

  common::Value GetMember(absl::string_view name) const override {
    return common::Value::FromError(
        internal::UnknownType(object_type().full_name()));
  };

  common::Type object_type() const override {
    return common::Type(FindObjectType(&holder_.value()));
  }

  void To(google::protobuf::Any* value) const override {
    *value = holder_.value();
  }

  bool owns_value() const override { return HolderPolicy::kOwnsValue; }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(
          absl::string_view, const common::Value&)>& call) const override {
    return internal::UnknownType(object_type().full_name());
  }

 protected:
  std::size_t ComputeHash() const override {
    return internal::Hash(object_type().full_name(), holder_->value());
  }

  bool EqualsImpl(const common::Object& same_type) const final {
    const UnrecognizedMessageObject* other =
        cast_if<UnrecognizedMessageObject>(&same_type);
    if (other == nullptr) {
      return false;
    }
    return GetDiffer().Equals(holder_.value(), other->holder_.value());
  }

 private:
  internal::Holder<google::protobuf::Any, HolderPolicy> holder_;
};

/**
 * A Object class for a proto message.
 */
template <typename HolderPolicy>
class MessageObject final : public common::Object {
 public:
  template <typename... Args>
  explicit MessageObject(const TypeRegistry* registry, Args&&... args)
      : registry_(registry), holder_(std::forward<Args>(args)...) {}

  template <common::Value::Kind ValueKind, class T>
  common::Value MakeList(const google::protobuf::RepeatedFieldRef<T>& value) const {
    return common::Value::MakeList<ProtoList<T, ValueKind>>(SelfRefProvider(),
                                                            value);
  }

  common::Value BuildMapFor(const google::protobuf::FieldDescriptor* field,
                            const google::protobuf::Message* msg) const {
    // Proto maps are represented by a repeated message with two fields
    // (key and value)
    absl::node_hash_map<common::Value, common::Value> result;
    const google::protobuf::Reflection* refl = msg->GetReflection();
    for (int index = 0; index < refl->FieldSize(*msg, field); ++index) {
      const auto& entry = refl->GetRepeatedMessage(*msg, field, index);
      const google::protobuf::FieldDescriptor* key_field =
          entry.GetDescriptor()->FindFieldByNumber(1);
      const google::protobuf::FieldDescriptor* value_field =
          entry.GetDescriptor()->FindFieldByNumber(2);
      result.emplace(GetFieldValue(key_field, &entry),
                     GetFieldValue(value_field, &entry));
    }
    // The keys and values grabbed a ref on parent if needed, so we don't need
    // one separately.
    return common::Value::MakeMap<expr::internal::MapImpl>(std::move(result));
  }

  common::Value GetFieldValue(const google::protobuf::FieldDescriptor* field,
                              const google::protobuf::Message* msg) const {
    const google::protobuf::Reflection* refl = msg->GetReflection();
    if (field->is_map()) {
      return BuildMapFor(field, msg);
    }
    if (field->is_repeated()) {
      switch (field->cpp_type()) {
        case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
          return MakeList<common::Value::Kind::kBool>(
              refl->GetRepeatedFieldRef<bool>(*msg, field));
        case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
          return MakeList<common::Value::Kind::kInt>(
              refl->GetRepeatedFieldRef<int32_t>(*msg, field));
        case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
          return MakeList<common::Value::Kind::kInt>(
              refl->GetRepeatedFieldRef<int64_t>(*msg, field));
        case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
          return common::Value::MakeList<ProtoEnumList>(
              registry_, SelfRefProvider(), msg, field);
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
          return MakeList<common::Value::Kind::kUInt>(
              refl->GetRepeatedFieldRef<uint32_t>(*msg, field));
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
          return MakeList<common::Value::Kind::kUInt>(
              refl->GetRepeatedFieldRef<uint64_t>(*msg, field));
        case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
          return MakeList<common::Value::Kind::kDouble>(
              refl->GetRepeatedFieldRef<float>(*msg, field));
        case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
          return MakeList<common::Value::Kind::kDouble>(
              refl->GetRepeatedFieldRef<double>(*msg, field));
        case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
          if (field->type() == google::protobuf::FieldDescriptor::TYPE_STRING) {
            return common::Value::MakeList<
                ProtoStrList<common::Value::Kind::kString>>(SelfRefProvider(),
                                                            msg, field);
          } else {
            return common::Value::MakeList<
                ProtoStrList<common::Value::Kind::kBytes>>(SelfRefProvider(),
                                                           msg, field);
          }
        case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
          return common::Value::MakeList<ProtoMsgList>(
              registry_, SelfRefProvider(), msg, field);
        default:
          return common::Value::FromError(internal::UnimplementedError(""));
      }
    }

    switch (field->cpp_type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        return common::Value::FromBool(refl->GetBool(*msg, field));
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        return common::Value::FromInt(refl->GetInt32(*msg, field));
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        return common::Value::FromInt(refl->GetInt64(*msg, field));
      case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        return registry_->ValueFrom(common::EnumType(field->enum_type()),
                                    refl->GetEnumValue(*msg, field));
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        return common::Value::FromUInt(refl->GetUInt32(*msg, field));
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        return common::Value::FromUInt(refl->GetUInt64(*msg, field));

      case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        return common::Value::FromDouble(refl->GetFloat(*msg, field));
      case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        return common::Value::FromDouble(refl->GetDouble(*msg, field));
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
        std::string scratch;
        const auto& value = refl->GetStringReference(*msg, field, &scratch);
        if (field->type() == google::protobuf::FieldDescriptor::TYPE_STRING) {
          if (&scratch == &value) {
            return common::Value::FromString(value);
          } else {
            return common::Value::ForString(value, SelfRefProvider());
          }
        } else {
          if (&scratch == &value) {
            return common::Value::FromBytes(value);
          } else {
            return common::Value::ForBytes(value, SelfRefProvider());
          }
        }
      }

      case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
        // Create value that holds on the parent instead of copying the
        // message value.
        const google::protobuf::Message* sub_msg = &refl->GetMessage(*msg, field);
        if (refl->HasField(*msg, field)) {
          return registry_->ValueFor(sub_msg, SelfRefProvider());
        }
        return registry_->GetDefault(sub_msg);
      }
      default:
        return common::Value::FromError(internal::UnimplementedError(""));
    }
  }

  common::Value GetMember(absl::string_view name) const override {
    std::string str_name(name);
    auto* field = holder_->GetDescriptor()->FindFieldByName(str_name);
    if (field == nullptr) {
      return common::Value::FromError(
          internal::NoSuchMember(name, holder_->GetDescriptor()->full_name()));
    }
    return GetFieldValue(field, &holder_.value());
  }

  common::Type object_type() const override {
    return common::Type(common::ObjectType(holder_.value().GetDescriptor()));
  }

  void To(google::protobuf::Any* value) const override {
    value->PackFrom(holder_.value());
  }

  bool owns_value() const override { return HolderPolicy::kOwnsValue; }

  common::Value ContainsMember(absl::string_view name) const override {
    std::string str_name(name);
    return common::Value::FromBool(
        holder_->GetDescriptor()->FindFieldByName(str_name) != nullptr);
  }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(
          absl::string_view, const common::Value&)>& call) const override {
    const google::protobuf::Descriptor* desc = holder_->GetDescriptor();
    for (int i = 0; i < desc->field_count(); ++i) {
      const auto* field_desc = desc->field(i);
      RETURN_IF_STATUS_ERROR(call(field_desc->name(),
                                  GetFieldValue(field_desc, &holder_.value())));
    }
    return internal::OkStatus();
  }

 protected:
  bool EqualsImpl(const Object& rhs) const override {
    const MessageObject* other = cast_if<MessageObject>(&rhs);
    if (other == nullptr) {
      return false;
    }
    return GetDiffer().Equals(holder_.value(), other->holder_.value());
  }

 private:
  const TypeRegistry* registry_;
  internal::Holder<const google::protobuf::Message, HolderPolicy> holder_;
};

template <typename T>
bool RegisterDefaultImpl(const common::Value& default_value, T* entry) {
  assert(default_value.is_value());
  if (entry->default_value.is_value()) {
    // Already registered.
    return false;
  }
  entry->default_value = default_value;
  return true;
}

template <typename T>
bool RegisterImpl(T&& new_ctor, T* existing_ctor) {
  assert(new_ctor != nullptr);
  if (*existing_ctor != nullptr) {
    // Already registered.
    return false;
  }
  *existing_ctor = std::forward<T>(new_ctor);
  return true;
}

}  // namespace

bool TypeRegistry::RegisterDefault(const common::ObjectType& object_type,
                                   const common::Value& default_value) {
  return RegisterDefaultImpl(default_value, &object_registry_[object_type]);
}

bool TypeRegistry::RegisterConstructor(
    const common::ObjectType& object_type,
    std::function<common::Value(const google::protobuf::Message&)> from_ctor) {
  return RegisterImpl(std::move(from_ctor),
                      &object_registry_[object_type].from_ctor);
}

bool TypeRegistry::RegisterConstructor(
    const common::ObjectType& object_type,
    std::function<common::Value(google::protobuf::Message&&)> from_ctor) {
  return RegisterImpl(std::move(from_ctor),
                      &object_registry_[object_type].from_move_ctor);
}

bool TypeRegistry::RegisterConstructor(
    const common::ObjectType& object_type,
    std::function<common::Value(std::unique_ptr<google::protobuf::Message>)> from_ctor) {
  return RegisterImpl(std::move(from_ctor),
                      &object_registry_[object_type].from_ptr_ctor);
}

bool TypeRegistry::RegisterConstructor(
    const common::ObjectType& object_type,
    std::function<common::Value(const google::protobuf::Message*)> for_ctor) {
  return RegisterImpl(std::move(for_ctor),
                      &object_registry_[object_type].for_ctor);
}

bool TypeRegistry::RegisterConstructor(
    const common::ObjectType& object_type,
    std::function<common::Value(const google::protobuf::Message*,
                                const common::RefProvider&)>
        for_ctor) {
  return RegisterImpl(std::move(for_ctor),
                      &object_registry_[object_type].for_pnt_ctor);
}

bool TypeRegistry::RegisterConstructor(
    const common::EnumType& enum_type,
    std::function<common::Value(common::EnumType, int32_t)> from_ctor) {
  return RegisterImpl(std::move(from_ctor),
                      &enum_registry_[enum_type].from_ctor);
}

common::Value TypeRegistry::GetDefault(
    const google::protobuf::Message* default_msg) const {
  common::ObjectType type(default_msg->GetDescriptor());
  if (type == common::ObjectType::For<google::protobuf::Any>()) {
    return common::Value::NullValue();
  }
  auto itr = object_registry_.find(type);
  if (itr != object_registry_.end()) {
    return itr->second.default_value.is_value() ? itr->second.default_value
                                                : common::Value::NullValue();
  }
  return ValueForUnregistered(default_msg);
}

common::Value TypeRegistry::ValueFrom(const google::protobuf::Message& value) const {
  common::ObjectType type(value.GetDescriptor());
  if (type == common::ObjectType::For<google::protobuf::Any>()) {
    return ValueFromAny(static_cast<const google::protobuf::Any&>(value));
  }

  auto entry = GetCalls(type);
  if (entry.from_ctor) {
    return entry.from_ctor(value);
  }

  // Try to fall back on other from calls.
  auto ptr = internal::Clone(value);
  if (entry.from_ptr_ctor) {
    return entry.from_ptr_ctor(std::move(ptr));
  } else if (entry.from_move_ctor) {
    return entry.from_move_ctor(std::move(*ptr));
  }

  if (entry.for_ctor || entry.for_pnt_ctor) {
    return MissingCall(type);
  }

  return ValueFromUnregistered(std::move(ptr));
}

common::Value TypeRegistry::ValueFrom(google::protobuf::Message&& value) const {
  common::ObjectType type(value.GetDescriptor());
  if (type == common::ObjectType::For<google::protobuf::Any>()) {
    return ValueFromAny(static_cast<google::protobuf::Any&&>(value));
  }

  auto entry = GetCalls(type);
  if (entry.from_move_ctor) {
    return entry.from_move_ctor(std::move(value));
  }

  // Try to fallback on other from calls.
  if (entry.from_ctor) {
    return entry.from_ctor(value);
  } else if (entry.from_ptr_ctor) {
    return entry.from_ptr_ctor(internal::Clone(std::move(value)));
  }

  if (entry.for_ctor || entry.for_pnt_ctor) {
    return MissingCall(type);
  }

  return ValueFromUnregistered(internal::Clone(std::move(value)));
}

common::Value TypeRegistry::ValueFrom(
    std::unique_ptr<google::protobuf::Message> value) const {
  common::ObjectType type(value->GetDescriptor());
  if (type == common::ObjectType::For<google::protobuf::Any>()) {
    return ValueFromAny(static_cast<const google::protobuf::Any&>(*value));
  }

  auto entry = GetCalls(type);
  if (entry.from_ptr_ctor != nullptr) {
    return entry.from_ptr_ctor(std::move(value));
  }

  // Try to fallback on other from_* calls.
  if (entry.from_move_ctor) {
    return entry.from_move_ctor(std::move(*value));
  } else if (entry.from_ctor) {
    return entry.from_ctor(*value);
  }

  if (entry.for_ctor || entry.for_pnt_ctor) {
    return MissingCall(type);
  }

  return ValueFromUnregistered(std::move(value));
}

common::Value TypeRegistry::ValueFor(const google::protobuf::Message* value,
                                     common::ParentRef parent) const {
  if (parent == absl::nullopt) {
    return ValueFrom(*value);
  }
  common::ObjectType type(value->GetDescriptor());
  if (type == common::ObjectType::For<google::protobuf::Any>()) {
    return ValueFromAny(static_cast<const google::protobuf::Any&>(*value));
  }

  ObjectRegistryEntry entry = GetCalls(type);
  // Try for_pnt_ctor.
  if (!parent->RequiresReference() && entry.for_ctor) {
    return entry.for_ctor(value);
  } else if (entry.for_pnt_ctor) {
    return entry.for_pnt_ctor(value, *parent);
  }

  // Try to fallback on from_* calls.
  if (entry.from_ctor != nullptr) {
    return entry.from_ctor(*value);
  } else if (entry.from_ptr_ctor != nullptr) {
    return entry.from_ptr_ctor(internal::Clone(*value));
  } else if (entry.from_move_ctor != nullptr) {
    auto ptr = internal::Clone(*value);
    return entry.from_move_ctor(std::move(*ptr));
  }

  if (entry.for_ctor) {
    return MissingCall(type);
  }
  return ValueForUnregistered(value, *parent);
}

common::Value TypeRegistry::ValueFrom(const common::EnumType& type,
                                      int32_t value) const {
  std::function<common::Value(common::EnumType type, int32_t value)> from_ctor;
  auto itr = enum_registry_.find(type);
  if (itr != enum_registry_.end()) {
    from_ctor = itr->second.from_ctor;
  }
  if (from_ctor) {
    return from_ctor(type, value);
  }

  return common::Value::FromInt(value);
}

common::Value TypeRegistry::ValueFromUnregistered(
    std::unique_ptr<google::protobuf::Message> value) const {
  return common::Value::MakeObject<MessageObject<internal::OwnedPtr>>(
      this, std::move(value));
}

common::Value TypeRegistry::ValueForUnregistered(
    const google::protobuf::Message* value, common::RefProvider parent) const {
  if (parent.RequiresReference()) {
    return common::Value::MakeObject<MessageObject<
        internal::ParentOwned<common::ValueRef, internal::UnownedPtr>>>(
        this, parent.GetRef(), value);
  }
  return common::Value::MakeObject<MessageObject<internal::UnownedPtr>>(this,
                                                                        value);
}

common::Value TypeRegistry::ValueFromAny(
    const google::protobuf::Any& value) const {
  common::Type type(FindObjectType(&value));
  if (!type.is_object()) {
    return common::Value::MakeObject<UnrecognizedMessageObject<internal::Copy>>(
        value);
  }
  auto unpacked = type.object_type().Unpack(value);
  if (unpacked == nullptr) {
    return common::Value::FromError(internal::ParseError(type.full_name()));
  }
  return ValueFrom(std::move(unpacked));
}

TypeRegistry::ObjectRegistryEntry TypeRegistry::GetCalls(
    const common::ObjectType& type) const {
  TypeRegistry::ObjectRegistryEntry result;
  auto itr = object_registry_.find(type);
  if (itr != object_registry_.end()) {
    result.from_ctor = itr->second.from_ctor;
    result.from_ptr_ctor = itr->second.from_ptr_ctor;
    result.from_move_ctor = itr->second.from_move_ctor;
    result.for_ctor = itr->second.for_ctor;
    result.for_pnt_ctor = itr->second.for_pnt_ctor;
  }
  return result;
}

}  // namespace protoutil
}  // namespace expr
}  // namespace api
}  // namespace google
