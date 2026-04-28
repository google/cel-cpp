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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_H_

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "google/protobuf/arena.h"

namespace cel {

class TypeCheckerBuilder;

// TypeChecker interface.
//
// Checks references and type agreement for a parsed CEL expression.
//
// See Compiler for bundled parse and type check from a source expression
// string.
class TypeChecker {
 public:
  virtual ~TypeChecker() = default;

  // Checks the references and type agreement of the given parsed expression
  // based on the configured CEL environment.
  //
  // Most type checking errors are returned as Issues in the validation result.
  // A non-ok status is returned if type checking can't reasonably complete
  // (e.g. if an internal precondition is violated or an extension returns an
  // error).
  absl::StatusOr<ValidationResult> Check(std::unique_ptr<Ast> ast) const;
  absl::StatusOr<ValidationResult> Check(std::unique_ptr<Ast> ast,
                                         google::protobuf::Arena* arena) const;
  absl::StatusOr<ValidationResult> Check(const Ast& ast) const;
  absl::StatusOr<ValidationResult> Check(const Ast& ast,
                                         google::protobuf::Arena* arena) const;

  // Returns a builder initialized with the configuration of this type checker.
  virtual std::unique_ptr<TypeCheckerBuilder> ToBuilder() const = 0;

 private:
  virtual absl::StatusOr<ValidationResult> CheckImpl(
      std::unique_ptr<Ast> ast, google::protobuf::Arena* absl_nullable arena) const = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_H_
