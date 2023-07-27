/*
 * Copyright 2022 Google LLC
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

#include "eval/public/portable_cel_expr_builder_factory.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/compiler/qualified_reference_resolver.h"
#include "eval/compiler/regex_precompilation_optimization.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "extensions/select_optimization.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

using cel::extensions::CreateSelectOptimizationProgramOptimizer;
using cel::extensions::SelectOptimizationAstUpdater;

std::unique_ptr<CelExpressionBuilder> CreatePortableExprBuilder(
    std::unique_ptr<LegacyTypeProvider> type_provider,
    const InterpreterOptions& options) {
  if (type_provider == nullptr) {
    ABSL_LOG(ERROR) << "Cannot pass nullptr as type_provider to "
                       "CreatePortableExprBuilder";
    return nullptr;
  }
  cel::RuntimeOptions runtime_options = ConvertToRuntimeOptions(options);
  auto builder =
      std::make_unique<CelExpressionBuilderFlatImpl>(runtime_options);

  builder->GetTypeRegistry()->RegisterTypeProvider(std::move(type_provider));

  FlatExprBuilder& flat_expr_builder = builder->flat_expr_builder();

  flat_expr_builder.AddAstTransform(NewReferenceResolverExtension(
      (options.enable_qualified_identifier_rewrites)
          ? ReferenceResolverOption::kAlways
          : ReferenceResolverOption::kCheckedOnly));
  // TODO(uncreated-issue/27): These need to be abstracted to avoid bringing in too
  // many build dependencies by default.
  flat_expr_builder.set_enable_comprehension_vulnerability_check(
      options.enable_comprehension_vulnerability_check);

  if (options.constant_folding) {
    builder->flat_expr_builder().AddProgramOptimizer(
        cel::ast_internal::CreateConstantFoldingExtension(
            options.constant_arena));
  }

  if (options.enable_regex_precompilation) {
    flat_expr_builder.AddProgramOptimizer(
        CreateRegexPrecompilationExtension(options.regex_max_program_size));
  }

  if (options.enable_select_optimization) {
    // Add AST transform to update select branches on a stored
    // CheckedExpression. This may already be performed by a type checker.
    flat_expr_builder.AddAstTransform(
        std::make_unique<SelectOptimizationAstUpdater>());
    // Add overloads for select optimization signature.
    // These are never bound, only used to prevent the builder from failing on
    // the overloads check.
    absl::Status status = builder->GetRegistry()->RegisterLazyFunction(
        CelFunctionDescriptor(cel::extensions::kCelAttribute, false,
                              {cel::Kind::kAny, cel::Kind::kList}));
    if (!status.ok()) {
      ABSL_LOG(ERROR) << "Failed to register @cel.attribute: " << status;
    }
    status = builder->GetRegistry()->RegisterLazyFunction(
        CelFunctionDescriptor(cel::extensions::kFieldsHas, false,
                              {cel::Kind::kAny, cel::Kind::kList}));
    if (!status.ok()) {
      ABSL_LOG(ERROR) << "Failed to register @cel.hasField: " << status;
    }
    // Add runtime implementation.
    flat_expr_builder.AddProgramOptimizer(
        CreateSelectOptimizationProgramOptimizer());
  }

  return builder;
}

}  // namespace google::api::expr::runtime
