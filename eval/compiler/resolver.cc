#include "eval/compiler/resolver.h"

#include <cstdint>
#include <string>

#include "google/protobuf/descriptor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

Resolver::Resolver(absl::string_view container,
                   const CelFunctionRegistry* function_registry,
                   const CelTypeRegistry* type_registry,
                   bool resolve_qualified_type_identifiers)
    : namespace_prefixes_(),
      enum_value_map_(),
      function_registry_(function_registry),
      type_registry_(type_registry),
      resolve_qualified_type_identifiers_(resolve_qualified_type_identifiers) {
  // The constructor for the registry determines the set of possible namespace
  // prefixes which may appear within the given expression container, and also
  // eagerly maps possible enum names to enum values.

  auto container_elements = absl::StrSplit(container, '.');
  std::string prefix = "";
  namespace_prefixes_.push_back(prefix);
  for (const auto& elem : container_elements) {
    // Tolerate trailing / leading '.'.
    if (elem.empty()) {
      continue;
    }
    absl::StrAppend(&prefix, elem, ".");
    namespace_prefixes_.insert(namespace_prefixes_.begin(), prefix);
  }

  for (const auto& prefix : namespace_prefixes_) {
    for (auto iter = type_registry->enums_map().begin();
         iter != type_registry->enums_map().end(); ++iter) {
      absl::string_view enum_name = iter->first;
      if (!absl::StartsWith(enum_name, prefix)) {
        continue;
      }

      auto remainder = absl::StripPrefix(enum_name, prefix);
      for (const auto& enumerator : iter->second) {
        // "prefixes" container is ascending-ordered. As such, we will be
        // assigning enum reference to the deepest available.
        // E.g. if both a.b.c.Name and a.b.Name are available, and
        // we try to reference "Name" with the scope of "a.b.c",
        // it will be resolved to "a.b.c.Name".
        auto key = absl::StrCat(remainder, !remainder.empty() ? "." : "",
                                enumerator.name);
        enum_value_map_[key] = CelValue::CreateInt64(enumerator.number);
      }
    }
  }
}

std::vector<std::string> Resolver::FullyQualifiedNames(absl::string_view name,
                                                       int64_t expr_id) const {
  // TODO(issues/105): refactor the reference resolution into this method.
  // and handle the case where this id is in the reference map as either a
  // function name or identifier name.
  std::vector<std::string> names;
  // Handle the case where the name contains a leading '.' indicating it is
  // already fully-qualified.
  if (absl::StartsWith(name, ".")) {
    std::string fully_qualified_name = std::string(name.substr(1));
    names.push_back(fully_qualified_name);
    return names;
  }

  // namespace prefixes is guaranteed to contain at least empty string, so this
  // function will always produce at least one result.
  for (const auto& prefix : namespace_prefixes_) {
    std::string fully_qualified_name = absl::StrCat(prefix, name);
    names.push_back(fully_qualified_name);
  }
  return names;
}

absl::optional<CelValue> Resolver::FindConstant(absl::string_view name,
                                                int64_t expr_id) const {
  auto names = FullyQualifiedNames(name, expr_id);
  for (const auto& name : names) {
    // Attempt to resolve the fully qualified name to a known enum.
    auto enum_entry = enum_value_map_.find(name);
    if (enum_entry != enum_value_map_.end()) {
      return enum_entry->second;
    }
    // Conditionally resolve fully qualified names as type values if the option
    // to do so is configured in the expression builder. If the type name is
    // not qualified, then it too may be returned as a constant value.
    if (resolve_qualified_type_identifiers_ || !absl::StrContains(name, ".")) {
      auto type_value = type_registry_->FindType(name);
      if (type_value.has_value()) {
        return *type_value;
      }
    }
  }
  return absl::nullopt;
}

std::vector<const CelFunction*> Resolver::FindOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types, int64_t expr_id) const {
  // Resolve the fully qualified names and then search the function registry
  // for possible matches.
  std::vector<const CelFunction*> funcs;
  auto names = FullyQualifiedNames(name, expr_id);
  for (auto it = names.begin(); it != names.end(); it++) {
    // Only one set of overloads is returned along the namespace hierarchy as
    // the function name resolution follows the same behavior as variable name
    // resolution, meaning the most specific definition wins. This is different
    // from how C++ namespaces work, as they will accumulate the overload set
    // over the namespace hierarchy.
    funcs = function_registry_->FindOverloads(*it, receiver_style, types);
    if (!funcs.empty()) {
      return funcs;
    }
  }
  return funcs;
}

std::vector<const CelFunctionProvider*> Resolver::FindLazyOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types, int64_t expr_id) const {
  // Resolve the fully qualified names and then search the function registry
  // for possible matches.
  std::vector<const CelFunctionProvider*> funcs;
  auto names = FullyQualifiedNames(name, expr_id);
  for (const auto& name : names) {
    funcs = function_registry_->FindLazyOverloads(name, receiver_style, types);
    if (!funcs.empty()) {
      return funcs;
    }
  }
  return funcs;
}

absl::optional<LegacyTypeAdapter> Resolver::FindTypeAdapter(
    absl::string_view name, int64_t expr_id) const {
  // Resolve the fully qualified names and then defer to the type registry
  // for possible matches.
  auto names = FullyQualifiedNames(name, expr_id);
  for (const auto& name : names) {
    auto maybe_adapter = type_registry_->FindTypeAdapter(name);
    if (maybe_adapter.has_value()) {
      return maybe_adapter;
    }
  }
  return absl::nullopt;
}

}  // namespace google::api::expr::runtime
