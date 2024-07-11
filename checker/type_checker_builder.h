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

#include "absl/status/statusor.h"
#include "checker/type_checker.h"

namespace cel {

// Builder for TypeChecker instances.
class TypeCheckerBuilder {
 public:
  TypeCheckerBuilder() = default;

  TypeCheckerBuilder(const TypeCheckerBuilder&) = delete;
  TypeCheckerBuilder& operator=(const TypeCheckerBuilder&) = delete;
  TypeCheckerBuilder(TypeCheckerBuilder&&) = delete;
  TypeCheckerBuilder& operator=(TypeCheckerBuilder&&) = delete;

  absl::StatusOr<std::unique_ptr<TypeChecker>> Build() &&;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_
