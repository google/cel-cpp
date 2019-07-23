// Helper base classes and function for custom objects.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_CUSTOM_OBJECT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CUSTOM_OBJECT_H_

#include "common/value.h"

namespace google {
namespace api {
namespace expr {
namespace common {

// An object base class for objects that do not support direct member access.
class OpaqueObject : public Object {
 public:
  // Does not contain any accessible members.
  Value GetMember(absl::string_view name) const final;
  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(absl::string_view, const Value&)>&
          call) const final;

  // Assume to own value (can be overridden).
  inline bool owns_value() const override { return true; }

  // Require a custom ToString function.
  std::string ToString() const override = 0;

 protected:
  // Require a custom hash function.
  std::size_t ComputeHash() const override = 0;
};

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_CUSTOM_OBJECT_H_
