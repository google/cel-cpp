// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECK_ENV_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECK_ENV_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "common/decl.h"

namespace cel::checker_internal {

class TypeCheckEnv;

// Helper class for managing nested scopes and the local variables they
// implicitly declare.
//
// Nested scopes have a lifetime dependency on any parent scopes and the
// parent Type environment. Nested scopes should generally be managed by
// unique_ptrs.
class VariableScope {
 public:
  explicit VariableScope(const TypeCheckEnv& env ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : env_(&env), parent_(nullptr) {}

  VariableScope(const VariableScope&) = delete;
  VariableScope& operator=(const VariableScope&) = delete;
  VariableScope(VariableScope&&) = default;
  VariableScope& operator=(VariableScope&&) = default;

  bool InsertVariableIfAbsent(VariableDecl decl) {
    return variables_.insert({decl.name(), std::move(decl)}).second;
  }

  std::unique_ptr<VariableScope> MakeNestedScope() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return absl::WrapUnique(new VariableScope(*env_, this));
  }

  absl::Nullable<const VariableDecl*> LookupVariable(
      absl::string_view name) const;

 private:
  VariableScope(const TypeCheckEnv& env ABSL_ATTRIBUTE_LIFETIME_BOUND,
                const VariableScope* parent ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : env_(&env), parent_(parent) {}

  absl::Nonnull<const TypeCheckEnv*> env_;
  absl::Nullable<const VariableScope*> parent_;
  absl::flat_hash_map<std::string, VariableDecl> variables_;
};

// Class managing the type check environment.
//
// Maintains lookup maps for variables and functions.
//
// This class is thread-compatible.
class TypeCheckEnv {
 private:
  using VariableDeclPtr = absl::Nonnull<const VariableDecl*>;
  using FunctionDeclPtr = absl::Nonnull<const FunctionDecl*>;

 public:
  TypeCheckEnv() : container_(""), parent_(nullptr) {};

  explicit TypeCheckEnv(const TypeCheckEnv* parent)
      : container_(parent != nullptr ? parent->container() : ""),
        parent_(parent) {}

  // Move-only.
  TypeCheckEnv(TypeCheckEnv&&) = default;
  TypeCheckEnv& operator=(TypeCheckEnv&&) = default;

  const std::string& container() const { return container_; }

  void set_container(std::string container) {
    container_ = std::move(container);
  }

  const absl::flat_hash_map<std::string, VariableDecl>& variables() const {
    return variables_;
  }

  // Inserts a variable declaration into the environment of the current scope if
  // is is not already present. Parent scopes are not searched.
  //
  // Returns true if the variable was inserted, false otherwise.
  bool InsertVariableIfAbsent(VariableDecl decl) {
    return variables_.insert({decl.name(), std::move(decl)}).second;
  }

  const absl::flat_hash_map<std::string, FunctionDecl>& functions() const {
    return functions_;
  }

  // Inserts a function declaration into the environment of the current scope if
  // is is not already present. Parent scopes are not searched (allowing for
  // shadowing).
  //
  // Returns true if the decl was inserted, false otherwise.
  bool InsertFunctionIfAbsent(FunctionDecl decl) {
    return functions_.insert({decl.name(), std::move(decl)}).second;
  }

  absl::Nullable<const TypeCheckEnv*> parent() const { return parent_; }
  void set_parent(TypeCheckEnv* parent) { parent_ = parent; }

  // Returns the declaration for the given name if it is found in the current
  // or any parent scope.
  // Note: the returned declaration ptr is only valid as long as no changes are
  // made to the environment.
  absl::Nullable<const VariableDecl*> LookupVariable(
      absl::string_view name) const;
  absl::Nullable<const FunctionDecl*> LookupFunction(
      absl::string_view name) const;

  TypeCheckEnv MakeExtendedEnvironment() const { return TypeCheckEnv(this); }
  VariableScope MakeVariableScope() const { return VariableScope(*this); }

 private:
  std::string container_;
  absl::Nullable<const TypeCheckEnv*> parent_;

  // Maps fully qualified names to declarations.
  absl::flat_hash_map<std::string, VariableDecl> variables_;
  absl::flat_hash_map<std::string, FunctionDecl> functions_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECK_ENV_H_
