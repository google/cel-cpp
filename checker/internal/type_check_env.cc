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

#include <cstddef>
#include <cstdint>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/constant.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {

const VariableDecl* absl_nullable TypeCheckEnv::LookupVariable(
    absl::string_view name) const {
  if (auto it = variables_.find(name); it != variables_.end()) {
    return &it->second;
  }
  return nullptr;
}

const FunctionDecl* absl_nullable TypeCheckEnv::LookupFunction(
    absl::string_view name) const {
  if (auto it = functions_.find(name); it != functions_.end()) {
    return &it->second;
  }

  return nullptr;
}

absl::StatusOr<absl::optional<Type>> TypeCheckEnv::LookupTypeName(
    absl::string_view name) const {
  for (auto iter = type_providers_.begin(); iter != type_providers_.end();
       ++iter) {
    CEL_ASSIGN_OR_RETURN(auto type, (*iter)->FindType(name));
    if (type.has_value()) {
      return type;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<VariableDecl>> TypeCheckEnv::LookupEnumConstant(
    absl::string_view type, absl::string_view value) const {
  for (auto iter = type_providers_.begin(); iter != type_providers_.end();
       ++iter) {
    CEL_ASSIGN_OR_RETURN(auto enum_constant,
                         (*iter)->FindEnumConstant(type, value));
    if (enum_constant.has_value()) {
      auto decl = MakeVariableDecl(absl::StrCat(enum_constant->type_full_name,
                                                ".", enum_constant->value_name),
                                   enum_constant->type);
      decl.set_value(Constant(static_cast<int64_t>(enum_constant->number)));
      return decl;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<VariableDecl>> TypeCheckEnv::LookupTypeConstant(
    google::protobuf::Arena* absl_nonnull arena, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(absl::optional<Type> type, LookupTypeName(name));
  if (type.has_value()) {
    return MakeVariableDecl(type->name(), TypeType(arena, *type));
  }

  if (name.find('.') != name.npos) {
    size_t last_dot = name.rfind('.');
    absl::string_view enum_name_candidate = name.substr(0, last_dot);
    absl::string_view value_name_candidate = name.substr(last_dot + 1);
    return LookupEnumConstant(enum_name_candidate, value_name_candidate);
  }

  return absl::nullopt;
}

absl::StatusOr<absl::optional<StructTypeField>> TypeCheckEnv::LookupStructField(
    absl::string_view type_name, absl::string_view field_name) const {
  // Check the type providers in registration order.
  // Note: this doesn't allow for shadowing a type with a subset type of the
  // same name -- the later type provider will still be considered when
  // checking field accesses.
  for (auto iter = type_providers_.begin(); iter != type_providers_.end();
       ++iter) {
    CEL_ASSIGN_OR_RETURN(
        auto field, (*iter)->FindStructTypeFieldByName(type_name, field_name));
    if (field.has_value()) {
      return field;
    }
  }
  return absl::nullopt;
}

const VariableDecl* absl_nullable VariableScope::LookupLocalVariable(
    absl::string_view name) const {
  const VariableScope* scope = this;
  while (scope != nullptr) {
    if (auto it = scope->variables_.find(name); it != scope->variables_.end()) {
      return &it->second;
    }
    scope = scope->parent_;
  }
  return nullptr;
}

}  // namespace cel::checker_internal
