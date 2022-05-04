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
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/cel_options.h"

namespace google::api::expr::runtime {

std::unique_ptr<CelExpressionBuilder> CreatePortableExprBuilder(
    std::unique_ptr<LegacyTypeProvider> type_provider,
    const InterpreterOptions& options) {
  if (type_provider == nullptr) {
    GOOGLE_LOG(ERROR) << "Cannot pass nullptr as type_provider to "
                  "CreatePortableExprBuilder";
    return nullptr;
  }
  auto builder = std::make_unique<FlatExprBuilder>();
  builder->GetTypeRegistry()->RegisterTypeProvider(std::move(type_provider));
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
  builder->set_enable_null_coercion(options.enable_null_to_message_coercion);
  builder->set_enable_wrapper_type_null_unboxing(
      options.enable_empty_wrapper_null_unboxing);
  builder->set_enable_heterogeneous_equality(
      options.enable_heterogeneous_equality);
  builder->set_enable_qualified_identifier_rewrites(
      options.enable_qualified_identifier_rewrites);

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

  return builder;
}

}  // namespace google::api::expr::runtime
