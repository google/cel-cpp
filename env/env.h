// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_ENV_ENV_H_
#define THIRD_PARTY_CEL_CPP_ENV_ENV_H_

#include <sys/types.h>

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "compiler/compiler.h"
#include "env/config.h"
#include "env/internal/ext_registry.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Env class establishes the environment for compiling CEL expressions.
//
// It is used to configure compiler options, extension functions, and other
// customizable CEL features.
class Env {
 public:
  // Registers a `CompilerLibrary` with the environment. Note that the library
  // does not automatically get added to a `Compiler`. `NewCompiler` relies
  // on `Config` to determine which libraries to load.
  void RegisterCompilerLibrary(
      absl::string_view name, absl::string_view alias, int version,
      absl::AnyInvocable<CompilerLibrary() const> library_factory) {
    extension_registry_.RegisterCompilerLibrary(name, alias, version,
                                                std::move(library_factory));
  }

  void SetDescriptorPool(
      std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool) {
    descriptor_pool_ = std::move(descriptor_pool);
  }

  const google::protobuf::DescriptorPool* GetDescriptorPool() const {
    return descriptor_pool_.get();
  }

  void SetConfig(const Config& config) { config_ = config; }

  absl::StatusOr<std::unique_ptr<Compiler>> NewCompiler();

 private:
  cel::env_internal::ExtensionRegistry extension_registry_;
  std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool_;
  CompilerOptions compiler_options_;
  Config config_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_ENV_ENV_H_
