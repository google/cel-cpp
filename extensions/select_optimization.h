// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_SELECT_OPTIMIZATION_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_SELECT_OPTIMIZATION_H_

#include "absl/status/status.h"
#include "base/ast_internal/ast_impl.h"
#include "eval/compiler/flat_expr_builder_extensions.h"

namespace cel::extensions {

constexpr char kCelAttribute[] = "cel.attribute";
constexpr char kFieldsHas[] = "fields.has";

// Scans ast for optimizable select branches.
//
// In general, this should be done by a type checker but may be deferred to
// runtime.
//
// This assumes the runtime type registry has the same definitions as the one
// used by the type checker.
class SelectOptimizationAstUpdater
    : public google::api::expr::runtime::AstTransform {
 public:
  SelectOptimizationAstUpdater() = default;

  absl::Status UpdateAst(google::api::expr::runtime::PlannerContext& context,
                         cel::ast_internal::AstImpl& ast) const override;
};

}  // namespace cel::extensions
#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_SELECT_OPTIMIZATION_H_
