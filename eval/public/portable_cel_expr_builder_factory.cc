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
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/compiler/qualified_reference_resolver.h"
#include "eval/public/cel_options.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

std::unique_ptr<CelExpressionBuilder> CreatePortableExprBuilder(
    std::unique_ptr<LegacyTypeProvider> type_provider,
    const InterpreterOptions& options) {
  if (type_provider == nullptr) {
    LOG(ERROR) << "Cannot pass nullptr as type_provider to "
                  "CreatePortableExprBuilder";
    return nullptr;
  }
  cel::RuntimeOptions runtime_options = ConvertToRuntimeOptions(options);
  auto builder = std::make_unique<FlatExprBuilder>(runtime_options);

  builder->GetTypeRegistry()->RegisterTypeProvider(std::move(type_provider));

  builder->AddAstTransform(NewReferenceResolverExtension(
      (options.enable_qualified_identifier_rewrites)
          ? ReferenceResolverOption::kAlways
          : ReferenceResolverOption::kCheckedOnly));
  // TODO(issues/5): These need to be abstracted to avoid bringing in too
  // many build dependencies by default.
  builder->set_enable_comprehension_vulnerability_check(
      options.enable_comprehension_vulnerability_check);
  builder->set_enable_regex_precompilation(options.enable_regex_precompilation);

  if (options.constant_folding && options.enable_updated_constant_folding) {
    builder->AddProgramOptimizer(
        cel::ast::internal::CreateConstantFoldingExtension(
            options.constant_arena));
  } else {
    builder->set_constant_folding(options.constant_folding,
                                  options.constant_arena);
  }

  return builder;
}

}  // namespace google::api::expr::runtime
