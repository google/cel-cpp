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

#include "checker/type_checker.h"

namespace cel {
absl::StatusOr<ValidationResult> TypeChecker::Check(
    std::unique_ptr<Ast> ast) const {
  return CheckImpl(std::move(ast), nullptr);
}

absl::StatusOr<ValidationResult> TypeChecker::Check(
    std::unique_ptr<Ast> ast, google::protobuf::Arena* arena) const {
  return CheckImpl(std::move(ast), arena);
}

absl::StatusOr<ValidationResult> TypeChecker::Check(const Ast& ast) const {
  return CheckImpl(std::make_unique<Ast>(ast), nullptr);
}

absl::StatusOr<ValidationResult> TypeChecker::Check(
    const Ast& ast, google::protobuf::Arena* arena) const {
  return CheckImpl(std::make_unique<Ast>(ast), arena);
}
}  // namespace cel
