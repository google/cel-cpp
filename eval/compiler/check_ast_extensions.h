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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CHECK_AST_EXTENSIONS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CHECK_AST_EXTENSIONS_H_

#include <vector>

#include "absl/status/statusor.h"
#include "common/ast.h"
#include "common/ast/metadata.h"

namespace google::api::expr::runtime {

// Extracts and validates extension tags from the AST `ast` that affect the
// runtime component. Returns the validated list of runtime extensions, or an
// error if there are multiple runtime extensions with the same ID.
absl::StatusOr<std::vector<cel::ExtensionSpec>>
ExtractAndValidateRuntimeExtensions(const cel::Ast& ast);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CHECK_AST_EXTENSIONS_H_
