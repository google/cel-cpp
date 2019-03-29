#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_MAP_IMPL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_MAP_IMPL_H_

#include "absl/container/node_hash_map.h"
#include "common/macros.h"
#include "common/value.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

/** A simple Value -> Value map implementation. */
class MapImpl final : public Map {
 public:
  explicit MapImpl(absl::node_hash_map<Value, Value>&& value)
      : value_(std::move(value)) {}

  inline std::size_t size() const override { return value_.size(); }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const Value&, const Value&)>&
          call) const override;

  inline bool owns_value() const override { return true; }

 protected:
  Value GetImpl(const Value& key) const override;

 private:
  absl::node_hash_map<Value, Value> value_;
};

template <Value::Kind KeyKind, Value::Kind ValueKind, typename T,
          typename HolderPolicy = Copy>
class MapWrapper : public Map {
 public:
  template <typename... Args>
  explicit MapWrapper(Args&&... args) : value_(std::forward<Args>(args)...) {}

  inline std::size_t size() const override { return value_->size(); }
  inline bool owns_value() const override { return HolderPolicy::kOwnsValue; }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const Value&, const Value&)>&
          call) const override;

 protected:
  Value GetImpl(const Value& key) const override;

 private:
  Holder<T, HolderPolicy> value_;
};

template <Value::Kind KeyKind, Value::Kind ValueKind, typename T,
          typename HolderPolicy>
Value MapWrapper<KeyKind, ValueKind, T, HolderPolicy>::GetImpl(
    const Value& key) const {
  auto* key_value = key.get_if<KeyKind>();
  if (key_value && representable_as<typename T::key_type>(*key_value)) {
    auto itr = value_->find(*key_value);
    if (itr != value_->end()) {
      return GetValue<ValueKind>(itr->second);
    }
  }
  return Value::FromError(NoSuchKey(key.ToString()));
}

template <Value::Kind KeyKind, Value::Kind ValueKind, typename T,
          typename HolderPolicy>
google::rpc::Status MapWrapper<KeyKind, ValueKind, T, HolderPolicy>::ForEach(
    const std::function<google::rpc::Status(const Value&, const Value&)>& call)
    const {
  for (const auto& entry : *value_) {
    RETURN_IF_STATUS_ERROR(call(GetValue<KeyKind>(entry.first),
                                GetValue<ValueKind>(entry.second)));
  }
  return OkStatus();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_MAP_IMPL_H_
