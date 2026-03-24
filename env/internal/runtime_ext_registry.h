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

#ifndef THIRD_PARTY_CEL_CPP_ENV_RUNTIME_EXT_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_ENV_RUNTIME_EXT_REGISTRY_H_

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace env_internal {

using FunctionRegistrationCallback = absl::AnyInvocable<absl::Status(
    RuntimeBuilder&, const RuntimeOptions&) const>;

// A registry for CEL runtime extension functions.
//
// Used to register runtime functions for extensions by name (or alias) and
// version.
class RuntimeExtensionRegistry {
 public:
  static constexpr int kLatest = std::numeric_limits<int>::max();

  void AddFunctionRegistration(
      absl::string_view name, absl::string_view alias, int version,
      FunctionRegistrationCallback function_registration_callback);

  absl::Status RegisterExtensionFunctions(RuntimeBuilder& runtime_builder,
                                          const RuntimeOptions& runtime_options,
                                          absl::string_view name,
                                          int version) const;

 private:
  class Registration final {
   public:
    Registration(absl::string_view name, absl::string_view alias, int version,
                 FunctionRegistrationCallback function_registration_callback)
        : name_(name),
          alias_(!alias.empty() ? alias : name),
          version_(version),
          function_registration_callback_(
              std::move(function_registration_callback)) {}

    absl::Status RegisterExtensionFunctions(
        RuntimeBuilder& runtime_builder,
        const RuntimeOptions& runtime_options) const {
      return function_registration_callback_(runtime_builder, runtime_options);
    }

   private:
    std::string name_;
    std::string alias_;
    int version_;
    FunctionRegistrationCallback function_registration_callback_;

    friend class RuntimeExtensionRegistry;
  };

  std::vector<RuntimeExtensionRegistry::Registration> registry_;
};

}  // namespace env_internal
}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_ENV_RUNTIME_EXT_REGISTRY_H_
