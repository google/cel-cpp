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

#include "internal/annotations.h"

#include <cstdint>
#include <utility>

#include "absl/log/absl_log.h"
#include "base/ast_internal/ast_impl.h"

namespace cel::internal {

using ::cel::ast_internal::AstImpl;

AnnotationMap BuildAnnotationMap(AstImpl& ast) {
  AnnotationMap annotation_exprs;
  // Caller validates that this is an annotated expression.
  auto& annotation_map = ast.root_expr().mutable_call_expr().mutable_args()[1];

  if (!annotation_map.has_map_expr()) {
    return annotation_exprs;
  }

  for (auto& entry : annotation_map.mutable_map_expr().mutable_entries()) {
    if (!entry.has_key() || !entry.key().has_const_expr() ||
        !entry.key().const_expr().has_int_value()) {
      continue;
    }
    int64_t id = entry.key().const_expr().int_value();
    if (!entry.has_value() || !entry.value().has_list_expr() ||
        entry.value().list_expr().elements().empty()) {
      continue;
    }
    annotation_exprs[id].reserve(entry.value().list_expr().elements().size());
    for (auto& element :
         entry.mutable_value().mutable_list_expr().mutable_elements()) {
      if (!element.expr().has_struct_expr() ||
          element.expr().struct_expr().name() != "cel.Annotation") {
        continue;
      }

      AnnotationRep rep{};

      for (auto& field :
           element.mutable_expr().mutable_struct_expr().mutable_fields()) {
        if (field.name() == "name") {
          rep.name = field.value().const_expr().string_value();
        } else if (field.name() == "inspect_only") {
          rep.inspect_only = field.value().const_expr().bool_value();
        } else if (field.name() == "value") {
          rep.value_expr = &field.mutable_value();
        }
      }

      if (rep.name.empty() ||
          (!rep.inspect_only && rep.value_expr == nullptr)) {
        ABSL_LOG(WARNING) << "Invalid annotation";
        // TODO - log error.
        continue;
      }
      annotation_exprs[id].push_back(std::move(rep));
    }
  }

  return annotation_exprs;
}

}  // namespace cel::internal
