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

#include "checker/internal/type_check_env.h"

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_factory.h"

namespace cel::checker_internal {

absl::Nullable<const VariableDecl*> TypeCheckEnv::LookupVariable(
    absl::string_view name) const {
  const TypeCheckEnv* scope = this;
  while (scope != nullptr) {
    if (auto it = scope->variables_.find(name); it != scope->variables_.end()) {
      return &it->second;
    }
    scope = scope->parent_;
  }
  return nullptr;
}

absl::Nullable<const FunctionDecl*> TypeCheckEnv::LookupFunction(
    absl::string_view name) const {
  const TypeCheckEnv* scope = this;
  while (scope != nullptr) {
    if (auto it = scope->functions_.find(name); it != scope->functions_.end()) {
      return &it->second;
    }
    scope = scope->parent_;
  }
  return nullptr;
}

absl::StatusOr<absl::optional<Type>> TypeCheckEnv::LookupTypeName(
    TypeFactory& type_factory, absl::string_view name) const {
  const TypeCheckEnv* scope = this;
  while (scope != nullptr) {
    for (auto iter = type_providers_.rbegin(); iter != type_providers_.rend();
         ++iter) {
      auto type = (*iter)->FindType(type_factory, name);
      if (!type.ok() || type->has_value()) {
        return type;
      }
    }
    scope = scope->parent_;
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<StructTypeField>> TypeCheckEnv::LookupStructField(
    TypeFactory& type_factory, absl::string_view type_name,
    absl::string_view field_name) const {
  const TypeCheckEnv* scope = this;
  while (scope != nullptr) {
    // Check the type providers in reverse registration order.
    // Note: this doesn't allow for shadowing a type with a subset type of the
    // same name -- the parent type provider will still be considered when
    // checking field accesses.
    for (auto iter = type_providers_.rbegin(); iter != type_providers_.rend();
         ++iter) {
      auto field_info = (*iter)->FindStructTypeFieldByName(
          type_factory, type_name, field_name);
      if (!field_info.ok() || field_info->has_value()) {
        return field_info;
      }
    }
    scope = scope->parent_;
  }
  return absl::nullopt;
}

absl::Nullable<const VariableDecl*> VariableScope::LookupVariable(
    absl::string_view name) const {
  const VariableScope* scope = this;
  while (scope != nullptr) {
    if (auto it = scope->variables_.find(name); it != scope->variables_.end()) {
      return &it->second;
    }
    scope = scope->parent_;
  }

  return env_->LookupVariable(name);
}

}  // namespace cel::checker_internal
