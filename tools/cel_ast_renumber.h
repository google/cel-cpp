// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_CEL_AST_RENUMBER_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_CEL_AST_RENUMBER_H_

#include <cstdint>

#include "google/api/expr/v1alpha1/checked.pb.h"

namespace cel::ast {

// Renumbers expression IDs in a CheckedExpr in-place.
// This is intended to be used for injecting multiple sub-expressions into
// a merged expression.
// TODO(issues/139): this does not renumber within macro_calls values.
// Returns the next free ID.
int64_t Renumber(int64_t starting_id, google::api::expr::v1alpha1::CheckedExpr* expr);

}  // namespace cel::ast

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_CEL_AST_RENUMBER_H_
