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

#include <memory>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast_internal/ast_impl.h"
#include "base/kind.h"
#include "common/memory.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/compiler/comprehension_vulnerability_check.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/qualified_reference_resolver.h"
#include "eval/compiler/regex_precompilation_optimization.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/select_optimization.h"
#include "internal/proto_util.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::MemoryManagerRef;
using ::cel::ast_internal::AstImpl;
using ::cel::extensions::CreateSelectOptimizationProgramOptimizer;
using ::cel::extensions::kCelAttribute;
using ::cel::extensions::kCelHasField;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel::extensions::SelectOptimizationAstUpdater;
using ::cel::runtime_internal::CreateConstantFoldingOptimizer;
using ::google::api::expr::internal::ValidateStandardMessageTypes;

// Adapter for a raw arena* pointer. Manages a MemoryManager object for the
// constant folding extension.
struct ArenaBackedConstfoldingFactory {
  MemoryManagerRef memory_manager;

  absl::StatusOr<std::unique_ptr<ProgramOptimizer>> operator()(
      PlannerContext& ctx, const AstImpl& ast) const {
    return CreateConstantFoldingOptimizer(memory_manager)(ctx, ast);
  }
};

}  // namespace

std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const google::protobuf::DescriptorPool* descriptor_pool,
    google::protobuf::MessageFactory* message_factory,
    const InterpreterOptions& options) {
  if (descriptor_pool == nullptr) {
    ABSL_LOG(ERROR) << "Cannot pass nullptr as descriptor pool to "
                       "CreateCelExpressionBuilder";
    return nullptr;
  }
  if (auto s = ValidateStandardMessageTypes(*descriptor_pool); !s.ok()) {
    ABSL_LOG(WARNING) << "Failed to validate standard message types: "
                      << s.ToString();  // NOLINT: OSS compatibility
    return nullptr;
  }

  cel::RuntimeOptions runtime_options = ConvertToRuntimeOptions(options);
  auto builder =
      std::make_unique<CelExpressionBuilderFlatImpl>(runtime_options);

  builder->GetTypeRegistry()
      ->InternalGetModernRegistry()
      .set_use_legacy_container_builders(options.use_legacy_container_builders);

  builder->GetTypeRegistry()->RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(descriptor_pool,
                                                   message_factory));

  FlatExprBuilder& flat_expr_builder = builder->flat_expr_builder();

  flat_expr_builder.AddAstTransform(NewReferenceResolverExtension(
      (options.enable_qualified_identifier_rewrites)
          ? ReferenceResolverOption::kAlways
          : ReferenceResolverOption::kCheckedOnly));

  if (options.enable_comprehension_vulnerability_check) {
    builder->flat_expr_builder().AddProgramOptimizer(
        CreateComprehensionVulnerabilityCheck());
  }

  if (options.constant_folding) {
    builder->flat_expr_builder().AddProgramOptimizer(
        ArenaBackedConstfoldingFactory{
            ProtoMemoryManagerRef(options.constant_arena)});
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
    absl::Status status =
        builder->GetRegistry()->RegisterLazyFunction(CelFunctionDescriptor(
            kCelAttribute, false, {cel::Kind::kAny, cel::Kind::kList}));
    if (!status.ok()) {
      ABSL_LOG(ERROR) << "Failed to register " << kCelAttribute << ": "
                      << status;
    }
    status = builder->GetRegistry()->RegisterLazyFunction(CelFunctionDescriptor(
        kCelHasField, false, {cel::Kind::kAny, cel::Kind::kList}));
    if (!status.ok()) {
      ABSL_LOG(ERROR) << "Failed to register " << kCelHasField << ": "
                      << status;
    }
    // Add runtime implementation.
    flat_expr_builder.AddProgramOptimizer(
        CreateSelectOptimizationProgramOptimizer());
  }

  return builder;
}

}  // namespace google::api::expr::runtime
