// Copyright 2026 Google LLC
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

#include "eval/compiler/check_ast_extensions.h"

#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/ast/metadata.h"

namespace google::api::expr::runtime {

absl::StatusOr<std::vector<cel::ExtensionSpec>>
ExtractAndValidateRuntimeExtensions(const cel::Ast& ast) {
  std::vector<cel::ExtensionSpec> runtime_extensions;
  absl::flat_hash_set<absl::string_view> seen_extension_ids;

  for (const cel::ExtensionSpec& extension : ast.source_info().extensions()) {
    bool is_runtime = false;
    for (const cel::ExtensionSpec::Component& component :
         extension.affected_components()) {
      if (component == cel::ExtensionSpec::Component::kRuntime) {
        is_runtime = true;
        break;
      }
    }

    if (!is_runtime) {
      continue;
    }

    if (!seen_extension_ids.insert(extension.id()).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("duplicate extension ID: ", extension.id()));
    }
    runtime_extensions.push_back(extension);
  }

  return runtime_extensions;
}

}  // namespace google::api::expr::runtime
