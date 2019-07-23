#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_LIST_IMPL_H_

#include "common/macros.h"
#include "common/value.h"
#include "internal/holder.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

// A wrapper for a native c++ list container.
template <common::Value::Kind ValueKind, typename T,
          typename HolderPolicy = Copy>
class ListWrapper : public common::List {
 public:
  template <typename... Args>
  explicit ListWrapper(Args&&... args) : value_(std::forward<Args>(args)...) {}

  inline std::size_t size() const override { return value_->size(); }
  inline bool owns_value() const override { return HolderPolicy::kOwnsValue; }

  common::Value Get(std::size_t index) const override;

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const common::Value&)>& call)
      const override;

 private:
  Holder<T, HolderPolicy> value_;
};

template <common::Value::Kind ValueKind, typename T, typename HolderPolicy>
common::Value ListWrapper<ValueKind, T, HolderPolicy>::Get(
    std::size_t index) const {
  return GetValue<ValueKind>((*value_)[index]);
}

template <common::Value::Kind ValueKind, typename T, typename HolderPolicy>
google::rpc::Status ListWrapper<ValueKind, T, HolderPolicy>::ForEach(
    const std::function<google::rpc::Status(const common::Value&)>& call)
    const {
  for (const auto& elem : *value_) {
    RETURN_IF_STATUS_ERROR(call(GetValue<ValueKind>(elem)));
  }
  return OkStatus();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_LIST_IMPL_H_
