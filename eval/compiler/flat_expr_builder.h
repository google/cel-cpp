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
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "base/ast.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class FlatExprBuilder {
 public:
  FlatExprBuilder(const cel::FunctionRegistry& function_registry,
                  const CelTypeRegistry& type_registry,
                  const cel::RuntimeOptions& options)
      : options_(options),
        function_registry_(function_registry),
        type_registry_(type_registry) {}

  // Create a flat expr builder with defaulted options.
  FlatExprBuilder(const cel::FunctionRegistry& function_registry,
                  const CelTypeRegistry& type_registry)
      : options_(cel::RuntimeOptions()),
        function_registry_(function_registry),
        type_registry_(type_registry) {}

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

  void AddProgramOptimizer(ProgramOptimizerFactory optimizer) {
    program_optimizers_.push_back(std::move(optimizer));
  }

  void set_container(std::string container) {
    container_ = std::move(container);
  }

  // TODO(uncreated-issue/45): Add overload for cref AST. At the moment, all the users
  // can pass ownership of a freshly converted AST.
  absl::StatusOr<FlatExpression> CreateExpressionImpl(
      std::unique_ptr<cel::ast::Ast> ast,
      std::vector<absl::Status>* warnings) const;

 private:
  cel::RuntimeOptions options_;
  std::string container_;
  // TODO(uncreated-issue/45): evaluate whether we should use a shared_ptr here to
  // allow built expressions to keep the registries alive.
  const cel::FunctionRegistry& function_registry_;
  const CelTypeRegistry& type_registry_;
  std::vector<std::unique_ptr<AstTransform>> ast_transforms_;
  std::vector<ProgramOptimizerFactory> program_optimizers_;

  bool enable_comprehension_vulnerability_check_ = false;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
