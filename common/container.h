// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_CONTAINER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CONTAINER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace cel {

// ExpressionContainer represents the namespace configuration for a CEL
// expression.
//
// The container defines the default resolution order for names referenced in
// the expression. It generally maps to a protobuf package and follows
// approximately the same resolution rules as protobuf or C++ namespaces.
//
// Aliases declare short names that can be referenced without resolving against
// the scopes defined by the container. For consistency, an alias cannot be
// a prefix of the container name. Aliases are always unqualified identifiers.
//
// An abbreviation is a special case of alias that behaves like an import or
// using declaration in other languages. (pkg.TypeName -> TypeName).
//
// For better traceability, prefer using abbreviations over aliases.
class ExpressionContainer {
 public:
  struct AliasListing {
    std::string alias;
    std::string name;

    bool IsAbbreviation() const;
  };

  ExpressionContainer() = default;

  static absl::StatusOr<ExpressionContainer> Create(absl::string_view name);

  ExpressionContainer(const ExpressionContainer&) = default;
  ExpressionContainer(ExpressionContainer&&) = default;
  ExpressionContainer& operator=(const ExpressionContainer&) = default;
  ExpressionContainer& operator=(ExpressionContainer&&) = default;

  // Returns the full name of the container.
  //
  // The default value is an empty string meaning no container.
  absl::string_view container() const { return container_; }

  // Sets the container name.
  //
  // Returns an error if the container name is malformed or conflicts with an
  // existing alias.
  absl::Status SetContainer(absl::string_view name);

  // Adds an abbreviation to the container.
  //
  // Returns an error if the abbreviation is malformed or conflicts with the
  // container or an existing alias.
  absl::Status AddAbbreviation(absl::string_view abrev);

  // Adds an alias to the container.
  //
  // Returns an error if the alias is malformed or conflicts with the container
  // or an existing alias.
  absl::Status AddAlias(absl::string_view alias, absl::string_view name);

  // Returns the full name of the alias or an empty string if not found.
  //
  // The returned string view may be invalidated by updates to the
  // ExpressionContainer.
  absl::string_view FindAlias(absl::string_view alias) const;

  // Utility method for listing the abbreviations in the container.
  // Order is not guaranteed.
  std::vector<std::string> ListAbbreviations() const;

  // Utility method for listing the aliases in the container.
  // Includes abbreviations.
  // Order is not guaranteed.
  std::vector<AliasListing> ListAliases() const;

  // Removes all aliases and abbreviations from the container.
  void clear() {
    container_.clear();
    aliases_.clear();
  }

 private:
  explicit ExpressionContainer(absl::string_view name);

  std::string container_;

  // alias -> full name.
  absl::flat_hash_map<std::string, std::string> aliases_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_CONTAINER_H_
