// Copyright 2025 Google LLC
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
//
// Utilities for extracting annotations from CEL ASTs.
#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_ANNOTATIONS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_ANNOTATIONS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "base/ast_internal/ast_impl.h"
#include "common/expr.h"

namespace cel::internal {

struct AnnotationRep {
  std::string name = "";
  bool inspect_only = false;
  Expr* value_expr = nullptr;
  // Internal index used to identify the program associated with this
  // annotation. Only used by the runtime.
  int index = -1;
};

// Note: this returns raw ptrs to the AST nodes for each annotation.
// These may be invalidated by any change to the underlying AST.
using AnnotationMap = absl::flat_hash_map<int64_t, std::vector<AnnotationRep>>;

AnnotationMap BuildAnnotationMap(ast_internal::AstImpl& ast);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_ANNOTATIONS_H_
