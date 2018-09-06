#include "protoutil/converters.h"

#include <string>
#include <utility>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/macros.h"
#include "internal/holder.h"
#include "internal/map_impl.h"
#include "internal/proto_util.h"
#include "internal/ref_countable.h"
#include "internal/status_util.h"

namespace google {
namespace api {
namespace expr {
namespace protoutil {

using ::google::api::expr::internal::OkStatus;

namespace {

template <typename T>
bool RegisterFromCall(TypeRegistry* registry) {
  return registry->RegisterConstructor<T>(
      [](const T& value) { return ValueFrom(value); });
}

template <typename T>
bool RegisterFromPtrCall(TypeRegistry* registry) {
  return registry->RegisterConstructor<T>(
      [](std::unique_ptr<T> value) { return ValueFrom(std::move(value)); });
}

template <typename T>
bool RegisterForCall(TypeRegistry* registry) {
  return registry->RegisterConstructor<T>(
      [](const T* value, const RefProvider& parent) {
        return ValueFor(value, parent);
      });
}

template <typename HolderPolicy>
class ListValue final : public List {
 public:
  template <typename... Args>
  explicit ListValue(Args&&... args) : holder_(std::forward<Args>(args)...) {}

  std::size_t size() const override { return holder_.value().values_size(); }

  expr::Value Get(std::size_t index) const override {
    if (index >= static_cast<std::size_t>(holder_.value().values_size())) {
      return expr::Value::FromError(
          internal::OutOfRangeError(index, holder_.value().values_size()));
    }
    return ValueFor(&holder_.value().values(index), SelfRefProvider());
  }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const expr::Value&)>& call)
      const override {
    for (const auto& elem : holder_.value().values()) {
      RETURN_IF_STATUS_ERROR(call(ValueFor(&elem, SelfRefProvider())));
    }
    return OkStatus();
  }

  bool owns_value() const override { return HolderPolicy::kOwnsValue; }

 private:
  internal::Holder<google::protobuf::ListValue, HolderPolicy> holder_;
};

template <typename HolderPolicy>
class Struct final : public Map {
 public:
  template <typename... Args>
  explicit Struct(Args&&... args) : holder_(std::forward<Args>(args)...) {}

  inline std::size_t size() const override {
    return holder_.value().fields_size();
  }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const Value&, const Value&)>&
          call) const override {
    for (const auto& field : holder_.value().fields()) {
      RETURN_IF_STATUS_ERROR(
          call(Value::ForString(field.first, SelfRefProvider()),
               ValueFor(field.second, SelfRefProvider())));
    }
    return internal::OkStatus();
  }

  inline bool owns_value() const override { return true; }

 protected:
  Value GetImpl(const Value& key) const override;

 private:
  internal::Holder<google::protobuf::Struct, HolderPolicy> holder_;
};

expr::Value BuildMapFor(const google::protobuf::Struct* struct_value,
                        ParentRef parent) {
  std::unordered_map<expr::Value, expr::Value> result;
  for (const auto& field : struct_value->fields()) {
    result.emplace(Value::ForString(field.first, parent),
                   ValueFor(&field.second, parent));
  }
  // The keys and values grabbed a ref on parent if needed, so we don't need one
  // separately.
  return Value::MakeMap<internal::MapImpl>(std::move(result));
}

expr::Value BuildMapFrom(google::protobuf::Struct&& struct_value) {
  std::unordered_map<expr::Value, expr::Value> result;
  for (auto& fields : *struct_value.mutable_fields()) {
    result.emplace(Value::FromString(fields.first),
                   ValueFrom(std::move(fields.second)));
  }
  return Value::MakeMap<internal::MapImpl>(std::move(result));
}

expr::Value BuildMapFrom(const google::protobuf::Struct& struct_value) {
  std::unordered_map<expr::Value, expr::Value> result;
  for (const auto& fields : struct_value.fields()) {
    result.emplace(Value::FromString(fields.first), ValueFrom(fields.second));
  }
  return Value::MakeMap<internal::MapImpl>(std::move(result));
}

}  // namespace

// Converters for google::protobuf::Value.
Value ValueFrom(const google::protobuf::Value& value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::kNullValue:
      return Value::NullValue();
    case google::protobuf::Value::kBoolValue:
      return Value::FromBool(value.bool_value());
    case google::protobuf::Value::kNumberValue:
      return Value::FromDouble(value.number_value());
    case google::protobuf::Value::kStringValue:
      return Value::FromString(value.string_value());
    case google::protobuf::Value::kStructValue:
      return ValueFrom(value.struct_value());
    default:
      return Value::FromError(
          internal::UnimplementedError(absl::StrCat(value.kind_case())));
  }
}

Value ValueFrom(google::protobuf::Value&& value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::kStructValue:
      return ValueFrom(absl::WrapUnique(value.release_struct_value()));
    case google::protobuf::Value::kListValue:
      return ValueFrom(absl::WrapUnique(value.release_list_value()));
    default:
      return ValueFrom(value);
  }
}

Value ValueFrom(std::unique_ptr<google::protobuf::Value> value) {
  return ValueFrom(std::move(*value));
}

Value ValueFor(const google::protobuf::Value* value, ParentRef parent) {
  switch (value->kind_case()) {
    case google::protobuf::Value::kStructValue:
      return ValueFor(&value->struct_value(), parent);
    case google::protobuf::Value::kListValue:
      return ValueFor(&value->list_value(), parent);
    default:
      return ValueFrom(*value);
  }
}

// Converters for google::protobuf::Struct.
Value ValueFrom(const google::protobuf::Struct& value) {
  return BuildMapFrom(value);
}

Value ValueFrom(std::unique_ptr<google::protobuf::Struct> value) {
  return BuildMapFrom(std::move(*value));
}
Value ValueFor(const google::protobuf::Struct* value, ParentRef parent) {
  return BuildMapFor(value, parent);
}

// Converters for google::protobuf::ListValue
Value ValueFrom(const google::protobuf::ListValue& value) {
  return Value::MakeList<ListValue<internal::Copy>>(value);
}
Value ValueFrom(std::unique_ptr<google::protobuf::ListValue> value) {
  return Value::MakeList<ListValue<internal::OwnedPtr>>(std::move(value));
}
Value ValueFor(const google::protobuf::ListValue* value, ParentRef parent) {
  if (!parent) {
    return ValueFrom(*value);
  }
  if (parent->RequiresReference()) {
    return Value::MakeList<
        ListValue<internal::ParentOwned<ValueRef, internal::UnownedPtr>>>(
        parent->GetRef(), value);
  }
  return Value::MakeList<ListValue<internal::UnownedPtr>>(value);
}

// Converters for time/duration.
Value ValueFrom(const google::protobuf::Timestamp& value) {
  return Value::FromTime(internal::DecodeTime(value));
}

Value ValueFrom(const google::protobuf::Duration& value) {
  return Value::FromDuration(internal::DecodeDuration(value));
}

bool RegisterConvertersWith(TypeRegistry* registry) {
  bool success = true;
  success &= registry->RegisterConstructor(
      EnumType(google::protobuf::NullValue_descriptor()),
      [](EnumType, int32_t) { return Value::NullValue(); });
  success &= RegisterFromCall<google::protobuf::Value>(registry);
  success &= RegisterFromPtrCall<google::protobuf::Value>(registry);
  success &= RegisterForCall<google::protobuf::Value>(registry);

  success &= RegisterFromCall<google::protobuf::Struct>(registry);
  success &= RegisterFromPtrCall<google::protobuf::Struct>(registry);
  success &= RegisterForCall<google::protobuf::Struct>(registry);

  success &= RegisterFromCall<google::protobuf::ListValue>(registry);
  success &= RegisterFromPtrCall<google::protobuf::ListValue>(registry);
  success &= RegisterForCall<google::protobuf::ListValue>(registry);

  success &= RegisterFromCall<google::protobuf::Duration>(registry);
  success &= RegisterFromCall<google::protobuf::Timestamp>(registry);
  success &= RegisterFromCall<google::protobuf::BoolValue>(registry);
  success &= RegisterFromCall<google::protobuf::Int32Value>(registry);
  success &= RegisterFromCall<google::protobuf::Int64Value>(registry);
  success &= RegisterFromCall<google::protobuf::UInt32Value>(registry);
  success &= RegisterFromCall<google::protobuf::UInt64Value>(registry);
  success &= RegisterFromCall<google::protobuf::FloatValue>(registry);
  success &= RegisterFromCall<google::protobuf::DoubleValue>(registry);
  success &= RegisterFromCall<google::protobuf::StringValue>(registry);
  success &= RegisterFromCall<google::protobuf::BytesValue>(registry);
  return success;
}

}  // namespace protoutil
}  // namespace expr
}  // namespace api
}  // namespace google
