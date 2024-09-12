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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "checker/internal/type_check_env.h"
#include "checker/type_checker.h"
#include "common/decl.h"

namespace cel {

class TypeCheckerBuilder;

using ConfigureBuilderCallback =
    absl::AnyInvocable<absl::Status(TypeCheckerBuilder&)>;

struct CheckerLibrary {
  // Optional identifier to avoid collisions re-adding the same declarations.
  // If id is empty, it is not considered.
  std::string id;
  // Functional implementation applying the library features to the builder.
  ConfigureBuilderCallback options;
};

// Builder for TypeChecker instances.
class TypeCheckerBuilder {
 public:
  TypeCheckerBuilder() = default;

  TypeCheckerBuilder(const TypeCheckerBuilder&) = delete;
  TypeCheckerBuilder& operator=(const TypeCheckerBuilder&) = delete;
  TypeCheckerBuilder(TypeCheckerBuilder&&) = delete;
  TypeCheckerBuilder& operator=(TypeCheckerBuilder&&) = delete;

  absl::StatusOr<std::unique_ptr<TypeChecker>> Build() &&;

  absl::Status AddLibrary(CheckerLibrary library);

  absl::Status AddVariable(const VariableDecl& decl);
  absl::Status AddFunction(const FunctionDecl& decl);

  void set_container(std::string container);

 private:
  std::vector<CheckerLibrary> libraries_;
  absl::flat_hash_set<std::string> library_ids_;

  checker_internal::TypeCheckEnv env_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_
