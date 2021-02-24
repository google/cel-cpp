#include "v1beta1/converters.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/code.pb.h"
#include "absl/container/node_hash_map.h"
#include "absl/strings/str_cat.h"
#include "common/converters.h"
#include "common/macros.h"
#include "internal/holder.h"
#include "internal/map_impl.h"
#include "internal/proto_util.h"
#include "internal/status_util.h"

namespace google {
namespace api {
namespace expr {
namespace v1beta1 {

using expr::internal::EncodeDuration;
using expr::internal::EncodeTime;
using expr::internal::IsOk;
using expr::internal::OkStatus;
using protoutil::TypeRegistry;

namespace {

/** A visitor that encodes ExprValues. */
struct ToExprValue {
  common::Value from;
  v1beta1::ExprValue* result;

  google::rpc::Status operator()(std::nullptr_t value) {
    result->mutable_value()->set_null_value(
        ::google::protobuf::NullValue::NULL_VALUE);
    return OkStatus();
  }

  google::rpc::Status operator()(bool value) {
    result->mutable_value()->set_bool_value(value);
    return OkStatus();
  }

  google::rpc::Status operator()(int64_t value) {
    result->mutable_value()->set_int64_value(value);
    return OkStatus();
  }

  google::rpc::Status operator()(uint64_t value) {
    result->mutable_value()->set_uint64_value(value);
    return OkStatus();
  }

  google::rpc::Status operator()(double value) {
    result->mutable_value()->set_double_value(value);
    return OkStatus();
  }

  google::rpc::Status operator()(const common::EnumValue& value) {
    auto enum_value = result->mutable_value()->mutable_enum_value();
    enum_value->set_value(value.value());
    enum_value->set_type(std::string(value.type().full_name()));
    return OkStatus();
  }

  google::rpc::Status operator()(absl::string_view value) {
    if (from.kind() == common::Value::Kind::kBytes) {
      result->mutable_value()->set_bytes_value(std::string(value));
    } else {
      result->mutable_value()->set_string_value(std::string(value));
    }
    return OkStatus();
  }

  bool CheckAndEncodeIfError(const google::rpc::Status& value) {
    if (value.code() != google::rpc::Code::OK) {
      *result->mutable_error()->add_errors() = value;
      return false;
    }
    return true;
  }

  google::rpc::Status StatusToRpcStatus(const absl::Status& value) {
    if (value.ok()) {
      return OkStatus();
    }
    google::rpc::Status error;
    error.set_code(static_cast<int>(value.code()));
    error.set_message(value.message().data(), value.message().size());
    return error;
  }

  void EncodeMessage(const google::protobuf::Message& value) {
    result->mutable_value()->mutable_object_value()->PackFrom(value);
  }

  google::rpc::Status EncodeValue(const common::Value& value,
                                  v1beta1::Value* sub_value) {
    ExprValue expr_value;
    auto status = ValueTo(value, &expr_value);
    if (IsOk(status)) {
      sub_value->Swap(expr_value.mutable_value());
      return status;
    }
    *result = expr_value;
    return status;
  }

  google::rpc::Status operator()(absl::Duration value) {
    google::protobuf::Duration duration;
    auto status = StatusToRpcStatus(EncodeDuration(value, &duration));
    if (CheckAndEncodeIfError(status)) {
      EncodeMessage(duration);
    }
    return status;
  }

  google::rpc::Status operator()(absl::Time value) {
    google::protobuf::Timestamp time;
    auto status = StatusToRpcStatus(EncodeTime(value, &time));
    if (CheckAndEncodeIfError(status)) {
      EncodeMessage(time);
    }
    return status;
  }

  google::rpc::Status operator()(const common::List& value) {
    auto& list_value = *result->mutable_value()->mutable_list_value();
    return value.ForEach([this, &list_value](const common::Value& elem) {
      return EncodeValue(elem, list_value.add_values());
    });
  }

  google::rpc::Status operator()(const common::Map& value) {
    auto& map_value = *result->mutable_value()->mutable_map_value();
    return value.ForEach([this, &map_value](const common::Value& key,
                                            const common::Value& value) {
      auto& entry = *map_value.add_entries();
      RETURN_IF_STATUS_ERROR(EncodeValue(key, entry.mutable_key()));
      return EncodeValue(value, entry.mutable_value());
    });
  }

  google::rpc::Status operator()(const common::Object& value) {
    value.To(result->mutable_value()->mutable_object_value());
    return OkStatus();
  }

  google::rpc::Status operator()(const common::Type& value) {
    result->mutable_value()->set_type_value(std::string(value.full_name()));
    return OkStatus();
  }

  google::rpc::Status operator()(const common::Unknown& value) {
    auto& unknown = *result->mutable_unknown();
    for (const auto& id : value.ids()) {
      unknown.add_exprs()->set_id(id.value());
    }
    return OkStatus();
  }

  google::rpc::Status operator()(const common::Error& value) {
    auto& error_set = *result->mutable_error();
    for (const auto& error : value.errors()) {
      *error_set.add_errors() = error;
    }
    return OkStatus();
  }
};

/**
 * Creates a new common::Value potentially with a reference on parent, if not
 * null.
 */
common::Value ValueFor(const v1beta1::Value* value, common::ParentRef parent,
                       const TypeRegistry* registry);

template <typename HolderPolicy>
class ListValue final : public common::List {
 public:
  template <typename... Args>
  explicit ListValue(const TypeRegistry* registry, Args&&... args)
      : registry_(registry), holder_(std::forward<Args>(args)...) {}

  std::size_t size() const override { return holder_.value().values_size(); }

  common::Value Get(std::size_t index) const override {
    if (index >= static_cast<std::size_t>(holder_.value().values_size())) {
      return common::Value::FromError(
          internal::OutOfRangeError(index, holder_.value().values_size()));
    }
    return ValueFor(&holder_.value().values(index), SelfRefProvider(),
                    registry_);
  }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const common::Value&)>& call)
      const override {
    auto ref = SelfRefProvider();
    for (const auto& elem : holder_.value().values()) {
      RETURN_IF_STATUS_ERROR(call(ValueFor(&elem, ref, registry_)));
    }
    return OkStatus();
  }

  bool owns_value() const override { return HolderPolicy::kOwnsValue; }

 private:
  const TypeRegistry* registry_;
  internal::Holder<v1beta1::ListValue, HolderPolicy> holder_;
};

using ListValueCopy = ListValue<internal::Copy>;
using ListValueOwned = ListValue<internal::OwnedPtr>;

common::Value BuildMapFor(const v1beta1::MapValue* map_value,
                          common::ParentRef parent,
                          const TypeRegistry* registry) {
  absl::node_hash_map<common::Value, common::Value> result;
  for (const auto& entry : map_value->entries()) {
    result.emplace(ValueFor(&entry.key(), parent, registry),
                   ValueFor(&entry.value(), parent, registry));
  }
  // The keys and values grabbed a ref on parent if needed, so we don't need one
  // separately.
  return common::Value::MakeMap<internal::MapImpl>(std::move(result));
}

common::Value BuildMapFrom(v1beta1::MapValue&& map_value,
                           const TypeRegistry* registry) {
  absl::node_hash_map<common::Value, common::Value> result;
  for (v1beta1::MapValue::Entry& entry : *map_value.mutable_entries()) {
    result.emplace(
        ValueFrom(absl::WrapUnique(entry.release_key()), registry),
        ValueFrom(absl::WrapUnique(entry.release_value()), registry));
  }
  return common::Value::MakeMap<internal::MapImpl>(std::move(result));
}

common::Value BuildMapFrom(const v1beta1::MapValue& map_value,
                           const TypeRegistry* registry) {
  absl::node_hash_map<common::Value, common::Value> result;
  for (auto& entry : map_value.entries()) {
    result.emplace(ValueFrom(entry.key(), registry),
                   ValueFrom(entry.value(), registry));
  }
  return common::Value::MakeMap<internal::MapImpl>(std::move(result));
}

}  // namespace

common::Value ValueFrom(const v1beta1::Value& value,
                        const TypeRegistry* registry) {
  switch (value.kind_case()) {
    case v1beta1::Value::kNullValue:
      return common::Value::NullValue();
    case v1beta1::Value::kBoolValue:
      return common::Value::FromBool(value.bool_value());
    case v1beta1::Value::kInt64Value:
      return common::Value::FromInt(value.int64_value());
    case v1beta1::Value::kUint64Value:
      return common::Value::FromUInt(value.uint64_value());
    case v1beta1::Value::kDoubleValue:
      return common::Value::FromDouble(value.double_value());
    case v1beta1::Value::kStringValue:
      return common::Value::FromString(value.string_value());
    case v1beta1::Value::kBytesValue:
      return common::Value::FromBytes(value.bytes_value());
    case v1beta1::Value::kTypeValue:
      return common::Value::FromType(value.type_value());
    case v1beta1::Value::kListValue:
      return common::Value::MakeList<ListValueCopy>(registry,
                                                    value.list_value());
    case v1beta1::Value::kObjectValue:
      return registry->ValueFrom(value.object_value());
    case v1beta1::Value::kMapValue:
      return BuildMapFrom(value.map_value(), registry);
    default:
      return common::Value::FromError(internal::UnimplementedError(
          absl::StrCat("Unimplemented value kind: ", value.kind_case())));
  }
}

common::Value ValueFrom(const v1beta1::ExprValue& value,
                        const TypeRegistry* registry) {
  switch (value.kind_case()) {
    case v1beta1::ExprValue::kValue:
      return ValueFrom(value.value(), registry);
    case v1beta1::ExprValue::kError:
      return common::Value::FromError(common::Error(value.error().errors()));
    case v1beta1::ExprValue::kUnknown: {
      std::vector<common::Id> ids;
      ids.reserve(value.unknown().exprs_size());
      for (const auto& id_ref : value.unknown().exprs()) {
        ids.emplace_back(id_ref.id());
      }
      return common::Value::FromUnknown(common::Unknown(ids));
    }
    default:
      return common::Value::FromError(internal::UnimplementedError(
          absl::StrCat("Unimplemented expr value kind: ", value.kind_case())));
  }
}

common::Value ValueFrom(v1beta1::Value&& value, const TypeRegistry* registry) {
  switch (value.kind_case()) {
    case v1beta1::Value::kListValue:
      return common::Value::MakeList<ListValueOwned>(
          registry, absl::WrapUnique(value.release_list_value()));
    case v1beta1::Value::kMapValue:
      return BuildMapFrom(std::move(*value.mutable_map_value()), registry);
    default:
      // All other cases do not take advantage of the rvalue.
      return ValueFrom(value, registry);
  }
}

common::Value ValueFrom(v1beta1::ExprValue&& value,
                        const TypeRegistry* registry) {
  switch (value.kind_case()) {
    case v1beta1::ExprValue::kValue:
      return ValueFrom(absl::WrapUnique(value.release_value()), registry);
    default:
      // All other cases cannot take advantage of the rvalue.
      return ValueFrom(value, registry);
  }
}

common::Value ValueFor(const v1beta1::ExprValue* value,
                       const TypeRegistry* registry) {
  switch (value->kind_case()) {
    case v1beta1::ExprValue::kValue:
      return ValueFor(&value->value(), registry);
    default:
      // All others can't take advantage of the unowned value.
      return ValueFrom(*value, registry);
  }
}

common::Value ValueFrom(std::unique_ptr<v1beta1::Value> value,
                        const TypeRegistry* registry) {
  return ValueFrom(std::move(*value), registry);
}

common::Value ValueFrom(std::unique_ptr<v1beta1::ExprValue> value,
                        const TypeRegistry* registry) {
  return ValueFrom(std::move(*value), registry);
}

common::Value ValueFor(const v1beta1::Value* value,
                       const TypeRegistry* registry) {
  return ValueFor(value, common::NoParent(), registry);
}

google::rpc::Status ValueTo(const common::Value& value,
                            v1beta1::ExprValue* result) {
  return value.visit(ToExprValue{value, result});
}

namespace {

common::Value ValueFor(const v1beta1::Value* value, common::ParentRef parent,
                       const TypeRegistry* registry) {
  if (parent == absl::nullopt) {
    return ValueFrom(*value, registry);
  }
  switch (value->kind_case()) {
    case v1beta1::Value::kListValue:
      if (parent->RequiresReference()) {
        return common::Value::MakeList<ListValue<
            internal::ParentOwned<common::ValueRef, internal::UnownedPtr>>>(
            registry, parent->GetRef(), &value->list_value());
      } else {
        return common::Value::MakeList<ListValue<internal::UnownedPtr>>(
            registry, &value->list_value());
      }
    case v1beta1::Value::kMapValue:
      return BuildMapFor(&value->map_value(), parent, registry);
    default:
      return ValueFrom(*value, registry);
  }
}

}  // namespace
}  // namespace v1beta1
}  // namespace expr
}  // namespace api
}  // namespace google
