/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_expression.h"

namespace google::api::expr::runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class FlatExprBuilder : public CelExpressionBuilder {
 public:
  FlatExprBuilder()
      : enable_unknowns_(false),
        enable_unknown_function_results_(false),
        enable_missing_attribute_errors_(false),
        shortcircuiting_(true),
        constant_folding_(false),
        constant_arena_(nullptr),
        enable_comprehension_(true),
        comprehension_max_iterations_(0),
        fail_on_warnings_(true),
        enable_qualified_type_identifiers_(false),
        enable_comprehension_list_append_(false),
        enable_comprehension_vulnerability_check_(false) {}

  // set_enable_unknowns controls support for unknowns in expressions created.
  void set_enable_unknowns(bool enabled) { enable_unknowns_ = enabled; }

  // set_enable_missing_attribute_errors support for error injection in
  // expressions created.
  void set_enable_missing_attribute_errors(bool enabled) {
    enable_missing_attribute_errors_ = enabled;
  }

  // set_enable_unknown_function_results controls support for unknown function
  // results.
  void set_enable_unknown_function_results(bool enabled) {
    enable_unknown_function_results_ = enabled;
  }

  // set_shortcircuiting regulates shortcircuiting of some expressions.
  // Be default shortcircuiting is enabled.
  void set_shortcircuiting(bool enabled) { shortcircuiting_ = enabled; }

  // Toggle constant folding optimization. By default it is not enabled.
  // The provided arena is used to hold the generated constants.
  void set_constant_folding(bool enabled, google::protobuf::Arena* arena) {
    constant_folding_ = enabled;
    constant_arena_ = arena;
  }

  void set_enable_comprehension(bool enabled) {
    enable_comprehension_ = enabled;
  }

  void set_comprehension_max_iterations(int max_iterations) {
    comprehension_max_iterations_ = max_iterations;
  }

  // Warnings (e.g. no function bound) fail immediately.
  void set_fail_on_warnings(bool should_fail) {
    fail_on_warnings_ = should_fail;
  }

  // set_enable_qualified_type_identifiers controls whether select expressions
  // may be treated as constant type identifiers during CelExpression creation.
  void set_enable_qualified_type_identifiers(bool enabled) {
    enable_qualified_type_identifiers_ = enabled;
  }

  // set_enable_comprehension_list_append controls whether the FlatExprBuilder
  // will attempt to optimize list concatenation within map() and filter()
  // macro comprehensions as an append of results on the `accu_var` rather than
  // as a reassignment of the `accu_var` to the concatenation of
  // `accu_var` + [elem].
  //
  // Before enabling, ensure that `#list_append` is not a function declared
  // within your runtime, and that your CEL expressions retain their integer
  // identifiers.
  //
  // This option is not safe for use with hand-rolled ASTs.
  void set_enable_comprehension_list_append(bool enabled) {
    enable_comprehension_list_append_ = enabled;
  }

  // set_enable_comprehension_vulnerability_check inspects comprehension
  // sub-expressions for the presence of potential memory exhaustion.
  //
  // Note: This flag is not necessary if you are only using Core CEL macros.
  //
  // Consider enabling this feature when using custom comprehensions, and
  // absolutely enable the feature when using hand-written ASTs for
  // comprehension expressions.
  void set_enable_comprehension_vulnerability_check(bool enabled) {
    enable_comprehension_vulnerability_check_ = enabled;
  }

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info,
      std::vector<absl::Status>* warnings) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::CheckedExpr* checked_expr) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::CheckedExpr* checked_expr,
      std::vector<absl::Status>* warnings) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpressionImpl(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info,
      const google::protobuf::Map<int64_t, google::api::expr::v1alpha1::Reference>* reference_map,
      std::vector<absl::Status>* warnings) const;

 private:
  bool enable_unknowns_;
  bool enable_unknown_function_results_;
  bool enable_missing_attribute_errors_;
  bool shortcircuiting_;

  bool constant_folding_;
  google::protobuf::Arena* constant_arena_;
  bool enable_comprehension_;
  int comprehension_max_iterations_;
  bool fail_on_warnings_;
  bool enable_qualified_type_identifiers_;
  bool enable_comprehension_list_append_;
  bool enable_comprehension_vulnerability_check_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
