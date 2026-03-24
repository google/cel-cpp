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

#include "env/internal/runtime_ext_registry.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace env_internal {

void RuntimeExtensionRegistry::AddFunctionRegistration(
    absl::string_view name, absl::string_view alias, int version,
    FunctionRegistrationCallback function_registration_callback) {
  registry_.push_back(Registration(name, alias, version,
                                   std::move(function_registration_callback)));
}

absl::Status RuntimeExtensionRegistry::RegisterExtensionFunctions(
    RuntimeBuilder& runtime_builder, const RuntimeOptions& runtime_options,
    absl::string_view name, int version) const {
  if (version == kLatest) {
    int max_version = -1;
    for (const Registration& registration : registry_) {
      if ((registration.name_ == name || registration.alias_ == name) &&
          registration.version_ > max_version) {
        max_version = registration.version_;
      }
    }
    if (max_version == -1) {
      return absl::NotFoundError(absl::StrCat(
          "Runtime functions are not registered for extension: ", name));
    }
    version = max_version;
  }
  for (const Registration& registration : registry_) {
    if ((registration.name_ == name || registration.alias_ == name) &&
        registration.version_ == version) {
      return registration.RegisterExtensionFunctions(runtime_builder,
                                                     runtime_options);
    }
  }

  return absl::NotFoundError(absl::StrCat(
      "Runtime functions are not registered for extension: ", name));
}
}  // namespace env_internal
}  // namespace cel
