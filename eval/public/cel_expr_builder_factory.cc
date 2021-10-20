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

#include "eval/public/cel_expr_builder_factory.h"

#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/cel_options.h"

namespace google::api::expr::runtime {

std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const InterpreterOptions& options) {
  auto builder = absl::make_unique<FlatExprBuilder>();
  builder->set_shortcircuiting(options.short_circuiting);
  builder->set_constant_folding(options.constant_folding,
                                options.constant_arena);
  builder->set_enable_comprehension(options.enable_comprehension);
  builder->set_enable_comprehension_list_append(
      options.enable_comprehension_list_append);
  builder->set_comprehension_max_iterations(
      options.comprehension_max_iterations);
  builder->set_fail_on_warnings(options.fail_on_warnings);
  builder->set_enable_qualified_type_identifiers(
      options.enable_qualified_type_identifiers);
  builder->set_enable_comprehension_vulnerability_check(
      options.enable_comprehension_vulnerability_check);

  switch (options.unknown_processing) {
    case UnknownProcessingOptions::kAttributeAndFunction:
      builder->set_enable_unknown_function_results(true);
      builder->set_enable_unknowns(true);
      break;
    case UnknownProcessingOptions::kAttributeOnly:
      builder->set_enable_unknowns(true);
      break;
    case UnknownProcessingOptions::kDisabled:
      break;
  }

  builder->set_enable_missing_attribute_errors(
      options.enable_missing_attribute_errors);

  return std::move(builder);
}

}  // namespace google::api::expr::runtime
