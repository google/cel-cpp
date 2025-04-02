// Copyright 2024 Google LLC
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
#include "checker/internal/type_checker_builder_impl.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "checker/internal/type_check_env.h"
#include "checker/internal/type_checker_impl.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {
namespace {

const absl::flat_hash_map<std::string, std::vector<Macro>>& GetStdMacros() {
  static const absl::NoDestructor<
      absl::flat_hash_map<std::string, std::vector<Macro>>>
      kStdMacros({
          {"has", {HasMacro()}},
          {"all", {AllMacro()}},
          {"exists", {ExistsMacro()}},
          {"exists_one", {ExistsOneMacro()}},
          {"filter", {FilterMacro()}},
          {"map", {Map2Macro(), Map3Macro()}},
          {"optMap", {OptMapMacro()}},
          {"optFlatMap", {OptFlatMapMacro()}},
      });
  return *kStdMacros;
}

absl::Status CheckStdMacroOverlap(const FunctionDecl& decl) {
  const auto& std_macros = GetStdMacros();
  auto it = std_macros.find(decl.name());
  if (it == std_macros.end()) {
    return absl::OkStatus();
  }
  const auto& macros = it->second;
  for (const auto& macro : macros) {
    bool macro_member = macro.is_receiver_style();
    size_t macro_arg_count = macro.argument_count() + (macro_member ? 1 : 0);
    for (const auto& ovl : decl.overloads()) {
      if (ovl.member() == macro_member &&
          ovl.args().size() == macro_arg_count) {
        return absl::InvalidArgumentError(absl::StrCat(
            "overload for name '", macro.function(), "' with ", macro_arg_count,
            " argument(s) overlaps with predefined macro"));
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status TypeCheckerBuilderImpl::AddContextDeclarationVariables(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  for (int i = 0; i < descriptor->field_count(); i++) {
    const google::protobuf::FieldDescriptor* proto_field = descriptor->field(i);
    MessageTypeField cel_field(proto_field);
    cel_field.name();
    Type field_type = cel_field.GetType();
    if (field_type.IsEnum()) {
      field_type = IntType();
    }
    if (!env_.InsertVariableIfAbsent(
            MakeVariableDecl(std::string(cel_field.name()), field_type))) {
      return absl::AlreadyExistsError(
          absl::StrCat("variable '", cel_field.name(),
                       "' already exists (from context declaration: '",
                       descriptor->full_name(), "')"));
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<TypeChecker>>
TypeCheckerBuilderImpl::Build() && {
  for (const auto* type : context_types_) {
    CEL_RETURN_IF_ERROR(AddContextDeclarationVariables(type));
  }

  auto checker = std::make_unique<checker_internal::TypeCheckerImpl>(
      std::move(env_), options_);
  return checker;
}

absl::Status TypeCheckerBuilderImpl::AddLibrary(CheckerLibrary library) {
  if (!library.id.empty() && !library_ids_.insert(library.id).second) {
    return absl::AlreadyExistsError(
        absl::StrCat("library '", library.id, "' already exists"));
  }
  if (!library.configure) {
    return absl::OkStatus();
  }
  absl::Status status = library.configure(*this);

  libraries_.push_back(std::move(library));
  return status;
}

absl::Status TypeCheckerBuilderImpl::AddVariable(const VariableDecl& decl) {
  bool inserted = env_.InsertVariableIfAbsent(decl);
  if (!inserted) {
    return absl::AlreadyExistsError(
        absl::StrCat("variable '", decl.name(), "' already exists"));
  }
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::AddContextDeclaration(
    absl::string_view type) {
  CEL_ASSIGN_OR_RETURN(absl::optional<Type> resolved_type,
                       env_.LookupTypeName(type));

  if (!resolved_type.has_value()) {
    return absl::NotFoundError(
        absl::StrCat("context declaration '", type, "' not found"));
  }

  if (!resolved_type->IsStruct()) {
    return absl::InvalidArgumentError(
        absl::StrCat("context declaration '", type, "' is not a struct"));
  }

  if (!resolved_type->AsStruct()->IsMessage()) {
    return absl::InvalidArgumentError(
        absl::StrCat("context declaration '", type,
                     "' is not protobuf message backed struct"));
  }

  const google::protobuf::Descriptor* descriptor =
      &(**(resolved_type->AsStruct()->AsMessage()));

  if (absl::c_linear_search(context_types_, descriptor)) {
    return absl::AlreadyExistsError(
        absl::StrCat("context declaration '", type, "' already exists"));
  }

  context_types_.push_back(descriptor);
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::AddFunction(const FunctionDecl& decl) {
  CEL_RETURN_IF_ERROR(CheckStdMacroOverlap(decl));
  bool inserted = env_.InsertFunctionIfAbsent(decl);
  if (!inserted) {
    return absl::AlreadyExistsError(
        absl::StrCat("function '", decl.name(), "' already exists"));
  }
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::MergeFunction(const FunctionDecl& decl) {
  const FunctionDecl* existing = env_.LookupFunction(decl.name());
  if (existing == nullptr) {
    return AddFunction(decl);
  }

  CEL_RETURN_IF_ERROR(CheckStdMacroOverlap(decl));

  FunctionDecl merged = *existing;

  for (const auto& overload : decl.overloads()) {
    if (!merged.AddOverload(overload).ok()) {
      return absl::AlreadyExistsError(
          absl::StrCat("function '", decl.name(),
                       "' already has overload that conflicts with overload ''",
                       overload.id(), "'"));
    }
  }

  env_.InsertOrReplaceFunction(std::move(merged));

  return absl::OkStatus();
}

void TypeCheckerBuilderImpl::AddTypeProvider(
    std::unique_ptr<TypeIntrospector> provider) {
  env_.AddTypeProvider(std::move(provider));
}

void TypeCheckerBuilderImpl::set_container(absl::string_view container) {
  env_.set_container(std::string(container));
}

void TypeCheckerBuilderImpl::SetExpectedType(const Type& type) {
  env_.set_expected_type(type);
}

}  // namespace cel::checker_internal
