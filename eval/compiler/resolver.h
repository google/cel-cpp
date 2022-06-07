#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_RESOLVER_H_

#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"

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
           const CelFunctionRegistry* function_registry,
           const CelTypeRegistry* type_registry,
           bool resolve_qualified_type_identifiers = true);

  ~Resolver() {}

  // FindConstant will return an enum constant value or a type value if one
  // exists for the given name.
  //
  // Since enums and type identifiers are specified as (potentially) qualified
  // names within an expression, there is the chance that the name provided
  // is a variable name which happens to collide with an existing enum or proto
  // based type name. For this reason, within parsed only expressions, the
  // constant should be treated as a value that can be shadowed by a runtime
  // provided value.
  absl::optional<CelValue> FindConstant(absl::string_view name,
                                        int64_t expr_id) const;

  // FindDescriptor returns the protobuf message descriptor for the given name
  // if one exists.
  const google::protobuf::Descriptor* FindDescriptor(absl::string_view name,
                                           int64_t expr_id) const;

  // FindTypeAdapter returns the adapter for the given type name if one exists,
  // following resolution rules for the expression container.
  absl::optional<LegacyTypeAdapter> FindTypeAdapter(absl::string_view name,
                                                    int64_t expr_id) const;

  // FindLazyOverloads returns the set, possibly empty, of lazy overloads
  // matching the given function signature.
  std::vector<const CelFunctionProvider*> FindLazyOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types, int64_t expr_id = -1) const;

  // FindOverloads returns the set, possibly empty, of eager function overloads
  // matching the given function signature.
  std::vector<const CelFunction*> FindOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types, int64_t expr_id = -1) const;

  // FullyQualifiedNames returns the set of fully qualified names which may be
  // derived from the base_name within the specified expression container.
  std::vector<std::string> FullyQualifiedNames(absl::string_view base_name,
                                               int64_t expr_id = -1) const;

 private:
  std::vector<std::string> namespace_prefixes_;
  absl::flat_hash_map<std::string, CelValue> enum_value_map_;
  const CelFunctionRegistry* function_registry_;
  const CelTypeRegistry* type_registry_;
  bool resolve_qualified_type_identifiers_;
};

// ArgumentMatcher generates a function signature matcher for CelFunctions.
// TODO(issues/91): this is the same behavior as parsed exprs in the CPP
// evaluator (just check the right call style and number of arguments), but we
// should have enough type information in a checked expr to  find a more
// specific candidate list.
inline std::vector<CelValue::Type> ArgumentsMatcher(int argument_count) {
  std::vector<CelValue::Type> argument_matcher(argument_count);
  for (int i = 0; i < argument_count; i++) {
    argument_matcher[i] = CelValue::Type::kAny;
  }
  return argument_matcher;
}

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_RESOLVER_H_
