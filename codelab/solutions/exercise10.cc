// Copyright 2025 Google LLC
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

#include "codelab/exercise10.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "codelab/network_functions.h"
#include "common/decl.h"
#include "common/minimal_descriptor_pool.h"
#include "common/type.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel_codelab {

namespace {

absl::StatusOr<std::unique_ptr<cel::Compiler>> ConfigureCompiler() {
  absl::StatusOr<std::unique_ptr<cel::CompilerBuilder>> compiler_builder =
      cel::NewCompilerBuilder(cel::GetMinimalDescriptorPool());
  if (!compiler_builder.ok()) {
    return std::move(compiler_builder).status();
  }
  absl::Status s =
      (*compiler_builder)->AddLibrary(cel::StandardCompilerLibrary());
  // ===========================================================================
  // Codelab: Update compiler builder with functions from network_functions.h
  // and add a varible for the input IP.
  // ===========================================================================
  s.Update((*compiler_builder)->AddLibrary(NetworkFunctionsCompilerLibrary()));
  s.Update((*compiler_builder)
               ->GetCheckerBuilder()
               .AddVariable(cel::MakeVariableDecl("ip", cel::StringType())));
  if (!s.ok()) return s;

  return (*compiler_builder)->Build();
}

absl::StatusOr<std::unique_ptr<cel::Runtime>> ConfigureRuntime() {
  cel::RuntimeOptions runtime_options;
  // Note: this is needed to resolve net.Address as a `type` constant.
  runtime_options.enable_qualified_type_identifiers = true;
  absl::StatusOr<cel::RuntimeBuilder> runtime_builder =
      cel::CreateStandardRuntimeBuilder(cel::GetMinimalDescriptorPool(),
                                        runtime_options);
  // ===========================================================================
  // Codelab: Update runtime builder with functions from network_functions.h
  // ===========================================================================
  absl::Status s =
      RegisterNetworkTypes(runtime_builder->type_registry(), runtime_options);
  s.Update(RegisterNetworkFunctions(runtime_builder->function_registry(),
                                    runtime_options));
  if (!s.ok()) return s;

  return std::move(runtime_builder).value().Build();
}

}  // namespace

absl::StatusOr<bool> CompileAndEvaluateExercise10(absl::string_view expression,
                                                  absl::string_view ip) {
  absl::StatusOr<std::unique_ptr<cel::Compiler>> compiler = ConfigureCompiler();
  if (!compiler.ok()) {
    return std::move(compiler).status();
  }

  absl::StatusOr<std::unique_ptr<cel::Runtime>> runtime = ConfigureRuntime();
  if (!runtime.ok()) {
    return std::move(runtime).status();
  }

  absl::StatusOr<cel::ValidationResult> checked =
      (*compiler)->Compile(expression);
  if (!checked.ok()) {
    return std::move(checked).status();
  }

  if (!checked->IsValid() || checked->GetAst() == nullptr) {
    return absl::InvalidArgumentError(checked->FormatError());
  }

  absl::StatusOr<std::unique_ptr<cel::Program>> program =
      (*runtime)->CreateProgram(checked->ReleaseAst().value());

  if (!program.ok()) {
    return std::move(program).status();
  }

  cel::Activation activation;
  google::protobuf::Arena arena;
  activation.InsertOrAssignValue("ip", cel::StringValue::From(ip, &arena));
  absl::StatusOr<cel::Value> result = (*program)->Evaluate(&arena, activation);

  if (!result.ok()) {
    return std::move(result).status();
  }

  if (result->IsBool()) {
    return result->GetBool();
  }

  if (result->IsError()) {
    return result->GetError().ToStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("unexpected result type: ", result->DebugString()));
}

}  // namespace cel_codelab
