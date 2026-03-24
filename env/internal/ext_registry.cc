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

#include "env/internal/ext_registry.h"

#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "compiler/compiler.h"

namespace cel {
namespace env_internal {

void ExtensionRegistry::RegisterCompilerLibrary(
    absl::string_view name, absl::string_view alias, int version,
    absl::AnyInvocable<CompilerLibrary() const> library_factory) {
  library_registry_.push_back(
      LibraryRegistration(name, alias, version, std::move(library_factory)));
}

absl::StatusOr<CompilerLibrary> ExtensionRegistry::GetCompilerLibrary(
    absl::string_view name, int version) const {
  if (version == kLatest) {
    int max_version = -1;
    for (const auto& registration : library_registry_) {
      if ((registration.name_ == name || registration.alias_ == name) &&
          registration.version_ > max_version) {
        max_version = registration.version_;
      }
    }
    if (max_version == -1) {
      return absl::NotFoundError(
          absl::StrCat("CompilerLibrary not registered: ", name));
    }
    version = max_version;
  }
  for (const auto& registration : library_registry_) {
    if ((registration.name_ == name || registration.alias_ == name) &&
        registration.version_ == version) {
      return registration.GetLibrary();
    }
  }

  return absl::NotFoundError(
      absl::StrCat("CompilerLibrary not registered: ", name, "#", version));
}
}  // namespace env_internal
}  // namespace cel
