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

#ifndef THIRD_PARTY_CEL_CPP_ENV_INTERNAL_EXT_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_ENV_INTERNAL_EXT_REGISTRY_H_

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "compiler/compiler.h"

namespace cel {
namespace env_internal {

// A registry for CEL compiler extension libraries.
//
// Used to register and retrieve CompilerLibraries by name (or alias) and
// version.
class ExtensionRegistry {
 public:
  static constexpr int kLatest = std::numeric_limits<int>::max();

  void RegisterCompilerLibrary(
      absl::string_view name, absl::string_view alias, int version,
      absl::AnyInvocable<CompilerLibrary() const> library_factory);

  absl::StatusOr<CompilerLibrary> GetCompilerLibrary(absl::string_view name,
                                                     int version) const;

 private:
  class LibraryRegistration final {
   public:
    LibraryRegistration(
        absl::string_view name, absl::string_view alias, int version,
        absl::AnyInvocable<CompilerLibrary() const> library_factory)
        : name_(name),
          alias_(!alias.empty() ? alias : name),
          version_(version),
          factory_(std::move(library_factory)) {}

    CompilerLibrary GetLibrary() const { return factory_(); }

   private:
    std::string name_;
    std::string alias_;
    int version_;
    absl::AnyInvocable<CompilerLibrary() const> factory_;

    friend class ExtensionRegistry;
  };

  std::vector<LibraryRegistration> library_registry_;
};

}  // namespace env_internal
}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_ENV_INTERNAL_EXT_REGISTRY_H_
