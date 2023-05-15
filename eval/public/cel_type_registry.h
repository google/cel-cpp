#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/descriptor.h"
#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/handle.h"
#include "base/types/enum_type.h"
#include "base/value.h"
#include "eval/public/structs/legacy_type_provider.h"

namespace google::api::expr::runtime {

// CelTypeRegistry manages the set of registered types available for use within
// object literal construction, enum comparisons, and type testing.
//
// The CelTypeRegistry is intended to live for the duration of all CelExpression
// values created by a given CelExpressionBuilder and one is created by default
// within the standard CelExpressionBuilder.
//
// By default, all core CEL types and all linked protobuf message types are
// implicitly registered by way of the generated descriptor pool. A descriptor
// pool can be given to avoid accidentally exposing linked protobuf types to CEL
// which were intended to remain internal or to operate on hermetic descriptor
// pools.
class CelTypeRegistry {
 public:
  // Representation of an enum constant.
  struct Enumerator {
    std::string name;
    int64_t number;
  };

  CelTypeRegistry();

  ~CelTypeRegistry() = default;

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

  // Register an enum whose values may be used within CEL expressions.
  //
  // Enum registration must be performed prior to CelExpression creation.
  void RegisterEnum(absl::string_view name,
                    std::vector<Enumerator> enumerators);

  // Register a new type provider.
  //
  // Type providers are consulted in the order they are added.
  void RegisterTypeProvider(std::unique_ptr<LegacyTypeProvider> provider) {
    type_providers_.push_back(std::move(provider));
  }

  // Get the first registered type provider.
  std::shared_ptr<const LegacyTypeProvider> GetFirstTypeProvider() const;

  // Find a type adapter given a fully qualified type name.
  // Adapter provides a generic interface for the reflection operations the
  // interpreter needs to provide.
  absl::optional<LegacyTypeAdapter> FindTypeAdapter(
      absl::string_view fully_qualified_type_name) const;

  // Find a type's CelValue instance by its fully qualified name.
  // An empty handle is returned if not found.
  cel::Handle<cel::Value> FindType(
      absl::string_view fully_qualified_type_name) const;

  // Return the set of enums configured within the type registry.
  ABSL_DEPRECATED("Use GetRegisteredEnums to validate RegisterEnum calls.")
  inline const absl::flat_hash_set<const google::protobuf::EnumDescriptor*>& Enums()
      const {
    return enums_;
  }

  // Return the registered enums configured within the type registry in the
  // internal format that can be identified as int constants at plan time.
  const absl::flat_hash_map<std::string, cel::Handle<cel::EnumType>>&
  resolveable_enums() const {
    return resolveable_enums_;
  }

  // Return the registered enums configured within the type registry.
  //
  // This is provided for validating registry setup, it should not be used
  // internally.
  //
  // Invalidated whenever registered enums are updated.
  absl::flat_hash_set<absl::string_view> ListResolveableEnums() const {
    absl::flat_hash_set<absl::string_view> result;
    result.reserve(resolveable_enums_.size());

    for (const auto& entry : resolveable_enums_) {
      result.insert(entry.first);
    }

    return result;
  }

 private:
  mutable absl::Mutex mutex_;
  // node_hash_set provides pointer-stability, which is required for the
  // strings backing CelType objects.
  mutable absl::node_hash_set<std::string> types_ ABSL_GUARDED_BY(mutex_);
  // Set of registered enums.
  absl::flat_hash_set<const google::protobuf::EnumDescriptor*> enums_;
  // Internal representation for enums.
  absl::flat_hash_map<std::string, cel::Handle<cel::EnumType>>
      resolveable_enums_;
  std::vector<std::shared_ptr<LegacyTypeProvider>> type_providers_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_
