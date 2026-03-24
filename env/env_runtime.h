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

#ifndef THIRD_PARTY_CEL_CPP_ENV_ENV_RUNTIME_H_
#define THIRD_PARTY_CEL_CPP_ENV_ENV_RUNTIME_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "env/config.h"
#include "env/internal/runtime_ext_registry.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// EnvRuntime class establishes the environment for creating CEL runtimes.
//
// It is used to configure runtime options, extension functions, and other
// customizable CEL runtime features.
//
// EnvRuntime is separate from Env to avoid a dependency on the compiler for
// binaries that only use the runtime.
//
// Even though EnvRuntime is separate from Env, the Config and DescriptorPool
// passed to EnvRuntime are expected to be the same as those passed to Env for
// compilation.  This ensures consistency between compilation and runtime.
class EnvRuntime {
 public:
  void SetDescriptorPool(
      std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool) {
    descriptor_pool_ = std::move(descriptor_pool);
  }

  void SetConfig(const Config& config) { config_ = config; }

  RuntimeOptions& mutable_runtime_options() { return runtime_options_; }

  absl::StatusOr<RuntimeBuilder> CreateRuntimeBuilder();

  // Shortcut for CreateRuntimeBuilder() followed by Build().
  absl::StatusOr<std::unique_ptr<Runtime>> NewRuntime();

 private:
  cel::env_internal::RuntimeExtensionRegistry& GetRuntimeExtensionRegistry() {
    return extension_registry_;
  }

  friend void RegisterStandardExtensions(EnvRuntime& env_runtime);

  cel::env_internal::RuntimeExtensionRegistry extension_registry_;
  std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool_;
  Config config_;
  RuntimeOptions runtime_options_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_ENV_ENV_RUNTIME_H_
