#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_RESOLVER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/kind.h"
#include "base/type_provider.h"
#include "base/value_factory.h"
#include "eval/public/cel_type_registry.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"

namespace google::api::expr::runtime {

// Resolver assists with finding functions and types within a container.
//
// This class builds on top of the CelFunctionRegistry and CelTypeRegistry by
// layering on the namespace resolution rules of CEL onto the calls provided
// by each of these libraries.
//
// TODO(issues/105): refactor the Resolver to consider CheckedExpr metadata
// for reference resolution.
class Resolver {
 public:
  Resolver(absl::string_view container,
           const cel::FunctionRegistry& function_registry,
           const CelTypeRegistry* type_registry,
           cel::ValueFactory& value_factory,
           const absl::flat_hash_map<std::string, cel::Handle<cel::EnumType>>&
               resolveable_enums,
           bool resolve_qualified_type_identifiers = true);

  ~Resolver() = default;

  // FindConstant will return an enum constant value or a type value if one
  // exists for the given name. An empty handle will be returned if none exists.
  //
  // Since enums and type identifiers are specified as (potentially) qualified
  // names within an expression, there is the chance that the name provided
  // is a variable name which happens to collide with an existing enum or proto
  // based type name. For this reason, within parsed only expressions, the
  // constant should be treated as a value that can be shadowed by a runtime
  // provided value.
  cel::Handle<cel::Value> FindConstant(absl::string_view name,
                                       int64_t expr_id) const;

  // FindTypeAdapter returns the adapter for the given type name if one exists,
  // following resolution rules for the expression container.
  absl::optional<LegacyTypeAdapter> FindTypeAdapter(absl::string_view name,
                                                    int64_t expr_id) const;

  // FindType returns the type and resolved type name for the given potentially
  // unqualified type name if one exists, following resolution rules for the
  // expression container.
  //
  // NOTE: The resolved type name is not necessarily the same as returned by
  // `cel::Type::name`, especially for some of the well known types. For example
  // `google.protobuf.Int32Value` and `google.protobuf.Int64Value` both resolve
  // to `IntWrapperType`.
  absl::StatusOr<absl::optional<std::pair<std::string, cel::Handle<cel::Type>>>>
  FindType(absl::string_view name, int64_t expr_id) const;

  // FindLazyOverloads returns the set, possibly empty, of lazy overloads
  // matching the given function signature.
  std::vector<cel::FunctionRegistry::LazyOverload> FindLazyOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<cel::Kind>& types, int64_t expr_id = -1) const;

  // FindOverloads returns the set, possibly empty, of eager function overloads
  // matching the given function signature.
  std::vector<cel::FunctionOverloadReference> FindOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<cel::Kind>& types, int64_t expr_id = -1) const;

  // FullyQualifiedNames returns the set of fully qualified names which may be
  // derived from the base_name within the specified expression container.
  std::vector<std::string> FullyQualifiedNames(absl::string_view base_name,
                                               int64_t expr_id = -1) const;

 private:
  std::vector<std::string> namespace_prefixes_;
  absl::flat_hash_map<std::string, cel::Handle<cel::Value>> enum_value_map_;
  const cel::FunctionRegistry& function_registry_;
  const CelTypeRegistry* type_registry_;
  cel::ValueFactory& value_factory_;
  const absl::flat_hash_map<std::string, cel::Handle<cel::EnumType>>&
      resolveable_enums_;

  bool resolve_qualified_type_identifiers_;
};

// ArgumentMatcher generates a function signature matcher for CelFunctions.
// TODO(issues/91): this is the same behavior as parsed exprs in the CPP
// evaluator (just check the right call style and number of arguments), but we
// should have enough type information in a checked expr to  find a more
// specific candidate list.
inline std::vector<cel::Kind> ArgumentsMatcher(int argument_count) {
  std::vector<cel::Kind> argument_matcher(argument_count);
  for (int i = 0; i < argument_count; i++) {
    argument_matcher[i] = cel::Kind::kAny;
  }
  return argument_matcher;
}

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_RESOLVER_H_
