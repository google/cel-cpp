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
#include "checker/type_checker_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/internal/type_check_env.h"
#include "checker/internal/type_checker_impl.h"
#include "checker/type_checker.h"
#include "common/decl.h"
#include "common/type_introspector.h"

namespace cel {

absl::StatusOr<std::unique_ptr<TypeChecker>> TypeCheckerBuilder::Build() && {
  if (env_.type_providers().empty() && env_.parent() == nullptr) {
    // Add a default type provider if none have been added to cover
    // WellKnownTypes.
    env_.AddTypeProvider(std::make_unique<TypeIntrospector>());
  }
  return std::make_unique<checker_internal::TypeCheckerImpl>(std::move(env_));
}

absl::Status TypeCheckerBuilder::AddLibrary(CheckerLibrary library) {
  if (!library.id.empty() && !library_ids_.insert(library.id).second) {
    return absl::AlreadyExistsError(
        absl::StrCat("library '", library.id, "' already exists"));
  }
  absl::Status status = library.options(*this);

  libraries_.push_back(std::move(library));
  return status;
}

absl::Status TypeCheckerBuilder::AddVariable(const VariableDecl& decl) {
  bool inserted = env_.InsertVariableIfAbsent(decl);
  if (!inserted) {
    return absl::AlreadyExistsError(
        absl::StrCat("variable '", decl.name(), "' already exists"));
  }
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilder::AddFunction(const FunctionDecl& decl) {
  bool inserted = env_.InsertFunctionIfAbsent(decl);
  if (!inserted) {
    return absl::AlreadyExistsError(
        absl::StrCat("function '", decl.name(), "' already exists"));
  }
  return absl::OkStatus();
}

void TypeCheckerBuilder::AddTypeProvider(
    std::unique_ptr<TypeIntrospector> provider) {
  env_.AddTypeProvider(std::move(provider));
}

void TypeCheckerBuilder::set_container(absl::string_view container) {
  env_.set_container(std::string(container));
}

}  // namespace cel
