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

#ifndef THIRD_PARTY_CEL_CPP_VALIDATOR_VALIDATOR_H_
#define THIRD_PARTY_CEL_CPP_VALIDATOR_VALIDATOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "checker/type_check_issue.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/navigable_ast.h"
namespace cel {

// Context for a validation pass.
//
// Assumed to be scoped to a Validator::Validate() call. Instances must not
// outlive the `ast` passed to the constructor.
class ValidationContext {
 public:
  explicit ValidationContext(const Ast& ast ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ast_(ast) {}

  const Ast& ast() const { return ast_; }
  const NavigableAst& navigable_ast() const {
    if (!navigable_ast_) {
      navigable_ast_ = NavigableAst::Build(ast_.root_expr());
    }
    return navigable_ast_;
  }

  void ReportWarningAt(int64_t id, absl::string_view message);
  void ReportErrorAt(int64_t id, absl::string_view message);
  void ReportWarning(absl::string_view message);
  void ReportError(absl::string_view message);

  std::vector<TypeCheckIssue> ReleaseIssues() {
    auto out = std::move(issues_);
    issues_.clear();
    return out;
  }

 private:
  const Ast& ast_;
  mutable NavigableAst navigable_ast_;
  std::vector<TypeCheckIssue> issues_;
};

// A single validation to apply to an AST.
//
// May be empty if default constructed or moved from.
// use operator bool() to check if the validation is empty.
class Validation {
 public:
  // Tests the AST reports any issues to the context.
  //
  // Returns false if the AST is invalid.
  //
  // The same instance is used across Validate() so must be thread safe
  // (typically stateless).
  using ImplFunction =
      absl::AnyInvocable<bool(ValidationContext& context) const>;

  Validation() = default;
  explicit Validation(ImplFunction impl);
  Validation(ImplFunction impl, absl::string_view id);

  const ImplFunction& impl() const {
    ABSL_DCHECK(rep_ != nullptr);
    return rep_->impl;
  }

  absl::string_view id() const {
    ABSL_DCHECK(rep_ != nullptr);
    return rep_->id;
  }

  bool operator()(ValidationContext& context) const {
    ABSL_DCHECK(rep_ != nullptr);
    return rep_->impl(context);
  }

  explicit operator bool() const { return rep_ != nullptr; }

 private:
  struct Rep {
    ImplFunction impl;
    // Optional id if supported in environment config.
    std::string id;
  };

  std::shared_ptr<const Rep> rep_;
};

// A validator checks a set of semantic rules for a given AST.
class Validator {
 public:
  Validator() = default;

  void AddValidation(Validation validation);
  absl::Span<const Validation> validations() const { return validations_; }

  struct ValidationOutput {
    bool valid = true;
    std::vector<TypeCheckIssue> issues;
  };

  // Validates the given AST by applying all of the validations.
  ValidationOutput Validate(const Ast& ast) const;

  // Validates the given AST, updating the validation result in place.
  //
  // Used to apply validators to the output of the type checker.
  void UpdateValidationResult(ValidationResult& in) const;

 private:
  std::vector<Validation> validations_;
};

// Implementation details.
inline Validation::Validation(ImplFunction impl)
    : rep_(std::make_shared<const Validation::Rep>(
          Validation::Rep{std::move(impl)})) {}

inline Validation::Validation(ImplFunction impl, absl::string_view id)
    : rep_(std::make_shared<const Validation::Rep>(
          Validation::Rep{std::move(impl), std::string(id)})) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_VALIDATOR_VALIDATOR_H_
