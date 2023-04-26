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

#include <memory>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/public/cel_expression.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class FlatExprBuilder : public CelExpressionBuilder {
 public:
  explicit FlatExprBuilder(const cel::RuntimeOptions& options)
      : CelExpressionBuilder(), options_(options) {}

  // Create a flat expr builder with defaulted options.
  FlatExprBuilder() : CelExpressionBuilder() {}

  // Toggle constant folding optimization. By default it is not enabled.
  // The provided arena is used to hold the generated constants.
  void set_constant_folding(bool enabled, google::protobuf::Arena* arena) {
    constant_folding_ = enabled;
    constant_arena_ = arena;
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

  void AddAstTransform(std::unique_ptr<AstTransform> transform) {
    ast_transforms_.push_back(std::move(transform));
  }

  void set_enable_regex_precompilation(bool enable) {
    enable_regex_precompilation_ = enable;
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

 private:
  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpressionImpl(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info,
      const google::protobuf::Map<int64_t, google::api::expr::v1alpha1::Reference>* reference_map,
      std::vector<absl::Status>* warnings) const;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpressionImpl(
      cel::ast::Ast& ast, std::vector<absl::Status>* warnings) const;

  cel::RuntimeOptions options_;
  std::vector<std::unique_ptr<AstTransform>> ast_transforms_;

  bool enable_regex_precompilation_ = false;
  bool enable_comprehension_vulnerability_check_ = false;
  bool constant_folding_ = false;
  google::protobuf::Arena* constant_arena_ = nullptr;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
