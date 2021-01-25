#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_

#include "google/protobuf/descriptor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelTypeRegistry manages the set of registered types available for use within
// object literal construction, enum comparisons, and type testing.
//
// The CelTypeRegistry is intended to live for the duration of all CelExpression
// values created by a given CelExpressionBuilder and one is created by default
// within the standard CelExpressionBuilder.
//
// By default, all core CEL types and all linked protobuf message types are
// implicitly registered by way of the generated descriptor pool. In the future,
// such type registrations may be explicit to avoid accidentally exposing linked
// protobuf types to CEL which were intended to remain internal.
class CelTypeRegistry {
 public:
  CelTypeRegistry();

  ~CelTypeRegistry() {}

  // Register a fully qualified type name as a valid type for use within CEL
  // expressions.
  //
  // This call establishes a CelValue type instance that can be used in runtime
  // comparisons, and may have implications in the future about which protobuf
  // message types linked into the binary may also be used by CEL.
  //
  // Type registration must be performed prior to CelExpression creation.
  void Register(std::string fully_qualified_type_name);

  // Register an enum whose values may be used within CEL expressions.
  //
  // Enum registration must be performed prior to CelExpression creation.
  void Register(const google::protobuf::EnumDescriptor* enum_descriptor);

  // Find a protobuf Descriptor given a fully qualified protobuf type name.
  const google::protobuf::Descriptor* FindDescriptor(
      absl::string_view fully_qualified_type_name) const;

  // Find a type's CelValue instance by its fully qualified name.
  absl::optional<CelValue> FindType(
      absl::string_view fully_qualified_type_name) const;

  // Return the set of enums configured within the type registry.
  inline const absl::flat_hash_set<const google::protobuf::EnumDescriptor*>& Enums()
      const {
    return enums_;
  }

 private:
  // pointer-stability is required for the strings in the types set, which is
  // why a node_hash_set is used instead of another container type.
  absl::node_hash_set<std::string> types_;
  absl::flat_hash_set<const google::protobuf::EnumDescriptor*> enums_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_
