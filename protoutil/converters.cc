#include "protoutil/converters.h"

#include <string>
#include <utility>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/container/node_hash_map.h"
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
      [](const T* value, const common::RefProvider& parent) {
        return ValueFor(value, parent);
      });
}

template <typename HolderPolicy>
class ListValue final : public common::List {
 public:
  template <typename... Args>
  explicit ListValue(Args&&... args) : holder_(std::forward<Args>(args)...) {}

  std::size_t size() const override { return holder_.value().values_size(); }

  common::Value Get(std::size_t index) const override {
    if (index >= static_cast<std::size_t>(holder_.value().values_size())) {
      return common::Value::FromError(
          internal::OutOfRangeError(index, holder_.value().values_size()));
    }
    return ValueFor(&holder_.value().values(index), SelfRefProvider());
  }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const common::Value&)>& call)
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
class Struct final : public common::Map {
 public:
  template <typename... Args>
  explicit Struct(Args&&... args) : holder_(std::forward<Args>(args)...) {}

  inline std::size_t size() const override {
    return holder_.value().fields_size();
  }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(
          const common::Value&, const common::Value&)>& call) const override {
    for (const auto& field : holder_.value().fields()) {
      RETURN_IF_STATUS_ERROR(
          call(common::Value::ForString(field.first, SelfRefProvider()),
               ValueFor(field.second, SelfRefProvider())));
    }
    return internal::OkStatus();
  }

  inline bool owns_value() const override { return true; }

 protected:
  common::Value GetImpl(const common::Value& key) const override;

 private:
  internal::Holder<google::protobuf::Struct, HolderPolicy> holder_;
};

common::Value BuildMapFor(const google::protobuf::Struct* struct_value,
                          common::ParentRef parent) {
  absl::node_hash_map<common::Value, common::Value> result;
  for (const auto& field : struct_value->fields()) {
    result.emplace(common::Value::ForString(field.first, parent),
                   ValueFor(&field.second, parent));
  }
  // The keys and values grabbed a ref on parent if needed, so we don't need one
  // separately.
  return common::Value::MakeMap<internal::MapImpl>(std::move(result));
}

common::Value BuildMapFrom(google::protobuf::Struct&& struct_value) {
  absl::node_hash_map<common::Value, common::Value> result;
  for (auto& fields : *struct_value.mutable_fields()) {
    result.emplace(common::Value::FromString(fields.first),
                   ValueFrom(std::move(fields.second)));
  }
  return common::Value::MakeMap<internal::MapImpl>(std::move(result));
}

common::Value BuildMapFrom(const google::protobuf::Struct& struct_value) {
  absl::node_hash_map<common::Value, common::Value> result;
  for (const auto& fields : struct_value.fields()) {
    result.emplace(common::Value::FromString(fields.first),
                   ValueFrom(fields.second));
  }
  return common::Value::MakeMap<internal::MapImpl>(std::move(result));
}

}  // namespace

// Converters for google::protobuf::Value.
common::Value ValueFrom(const google::protobuf::Value& value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::kNullValue:
      return common::Value::NullValue();
    case google::protobuf::Value::kBoolValue:
      return common::Value::FromBool(value.bool_value());
    case google::protobuf::Value::kNumberValue:
      return common::Value::FromDouble(value.number_value());
    case google::protobuf::Value::kStringValue:
      return common::Value::FromString(value.string_value());
    case google::protobuf::Value::kStructValue:
      return ValueFrom(value.struct_value());
    default:
      return common::Value::FromError(
          internal::UnimplementedError(absl::StrCat(value.kind_case())));
  }
}

common::Value ValueFrom(google::protobuf::Value&& value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::kStructValue:
      return ValueFrom(absl::WrapUnique(value.release_struct_value()));
    case google::protobuf::Value::kListValue:
      return ValueFrom(absl::WrapUnique(value.release_list_value()));
    default:
      return ValueFrom(value);
  }
}

common::Value ValueFrom(std::unique_ptr<google::protobuf::Value> value) {
  return ValueFrom(std::move(*value));
}

common::Value ValueFor(const google::protobuf::Value* value,
                       common::ParentRef parent) {
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
common::Value ValueFrom(const google::protobuf::Struct& value) {
  return BuildMapFrom(value);
}

common::Value ValueFrom(std::unique_ptr<google::protobuf::Struct> value) {
  return BuildMapFrom(std::move(*value));
}
common::Value ValueFor(const google::protobuf::Struct* value,
                       common::ParentRef parent) {
  return BuildMapFor(value, parent);
}

// Converters for google::protobuf::ListValue
common::Value ValueFrom(const google::protobuf::ListValue& value) {
  return common::Value::MakeList<ListValue<internal::Copy>>(value);
}
common::Value ValueFrom(std::unique_ptr<google::protobuf::ListValue> value) {
  return common::Value::MakeList<ListValue<internal::OwnedPtr>>(
      std::move(value));
}
common::Value ValueFor(const google::protobuf::ListValue* value,
                       common::ParentRef parent) {
  if (!parent) {
    return ValueFrom(*value);
  }
  if (parent->RequiresReference()) {
    return common::Value::MakeList<ListValue<
        internal::ParentOwned<common::ValueRef, internal::UnownedPtr>>>(
        parent->GetRef(), value);
  }
  return common::Value::MakeList<ListValue<internal::UnownedPtr>>(value);
}

// Converters for time/duration.
common::Value ValueFrom(const google::protobuf::Timestamp& value) {
  return common::Value::FromTime(internal::DecodeTime(value));
}

common::Value ValueFrom(const google::protobuf::Duration& value) {
  return common::Value::FromDuration(internal::DecodeDuration(value));
}

bool RegisterConvertersWith(TypeRegistry* registry) {
  bool success = true;
  success &= registry->RegisterConstructor(
      common::EnumType(google::protobuf::NullValue_descriptor()),
      [](common::EnumType, int32_t) { return common::Value::NullValue(); });
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
