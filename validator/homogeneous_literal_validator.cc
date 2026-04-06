// Copyright 2024 Google LLC
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

#include "validator/homogeneous_literal_validator.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/expr.h"
#include "common/navigable_ast.h"
#include "validator/validator.h"

namespace cel {

namespace {

bool InExemptFunction(const NavigableAstNode& node,
                      const std::vector<std::string>& exempt_functions) {
  const NavigableAstNode* parent = node.parent();
  while (parent != nullptr) {
    if (parent->node_kind() == NodeKind::kCall) {
      absl::string_view fn_name = parent->expr()->call_expr().function();
      for (const auto& exempt : exempt_functions) {
        if (exempt == fn_name) {
          return true;
        }
      }
    }
    parent = parent->parent();
  }
  return false;
}

bool IsOptional(const TypeSpec& t) {
  return t.has_abstract_type() && t.abstract_type().name() == "optional_type";
}

const TypeSpec& GetOptionalParameter(const TypeSpec& t) {
  return t.abstract_type().parameter_types()[0];
}

void TypeMismatch(ValidationContext& context, int64_t id,
                  const TypeSpec& expected, const TypeSpec& actual) {
  context.ReportErrorAt(
      id, absl::StrCat("expected type '", FormatTypeSpec(expected),
                       "' but found '", FormatTypeSpec(actual), "'"));
}

bool TypeEquiv(const TypeSpec& a, const TypeSpec& b) {
  if (a == b) {
    return true;
  }

  if (a.has_error() || b.has_error()) {
    // Don't report mismatch if there's an error (type checking failed for the
    // expression).
    return true;
  }

  if (a.has_wrapper() && b.has_primitive()) {
    return a.wrapper() == b.primitive();
  } else if (a.has_primitive() && b.has_wrapper()) {
    return a.primitive() == b.wrapper();
  }

  if (a.has_list_type() && b.has_list_type()) {
    return TypeEquiv(a.list_type().elem_type(), b.list_type().elem_type());
  }

  if (a.has_map_type() && b.has_map_type()) {
    return TypeEquiv(a.map_type().key_type(), b.map_type().key_type()) &&
           TypeEquiv(a.map_type().value_type(), b.map_type().value_type());
  }

  if (a.has_abstract_type() && b.has_abstract_type() &&
      a.abstract_type().name() == b.abstract_type().name() &&
      a.abstract_type().parameter_types().size() ==
          b.abstract_type().parameter_types().size()) {
    for (int i = 0; i < a.abstract_type().parameter_types().size(); ++i) {
      if (!TypeEquiv(a.abstract_type().parameter_types()[i],
                     b.abstract_type().parameter_types()[i])) {
        return false;
      }
    }
    return true;
  }

  return false;
}

}  // namespace

Validation HomogeneousLiteralValidator(
    std::vector<std::string> exempt_functions) {
  return Validation([exempt_functions = std::move(exempt_functions)](
                        ValidationContext& context) -> bool {
    bool valid = true;
    for (const auto& node :
         context.navigable_ast().Root().DescendantsPostorder()) {
      if (node.node_kind() == NodeKind::kList) {
        if (InExemptFunction(node, exempt_functions)) {
          continue;
        }
        const auto& list_expr = node.expr()->list_expr();
        const auto& elements = list_expr.elements();
        const TypeSpec* expected_type = nullptr;

        for (const auto& element : elements) {
          int64_t id = element.expr().id();
          const TypeSpec& actual_type = context.ast().GetTypeOrDyn(id);
          const TypeSpec* type_to_check = &actual_type;

          if (element.optional() && IsOptional(actual_type)) {
            type_to_check = &GetOptionalParameter(actual_type);
          }

          if (expected_type == nullptr) {
            expected_type = type_to_check;
            continue;
          }

          if (!(TypeEquiv(*expected_type, *type_to_check))) {
            TypeMismatch(context, id, *expected_type, *type_to_check);
            valid = false;
            break;
          }
        }
      } else if (node.node_kind() == NodeKind::kMap) {
        if (InExemptFunction(node, exempt_functions)) {
          continue;
        }
        const auto& map_expr = node.expr()->map_expr();
        const auto& entries = map_expr.entries();
        const TypeSpec* expected_key_type = nullptr;
        const TypeSpec* expected_value_type = nullptr;

        for (const auto& entry : entries) {
          int64_t key_id = entry.key().id();
          int64_t val_id = entry.value().id();
          const TypeSpec& actual_key_type = context.ast().GetTypeOrDyn(key_id);
          const TypeSpec& actual_val_type = context.ast().GetTypeOrDyn(val_id);
          const TypeSpec* key_type_to_check = &actual_key_type;
          const TypeSpec* val_type_to_check = &actual_val_type;

          if (entry.optional() && IsOptional(actual_val_type)) {
            val_type_to_check = &GetOptionalParameter(actual_val_type);
          }

          if (expected_key_type == nullptr) {
            expected_key_type = key_type_to_check;
            expected_value_type = val_type_to_check;
            continue;
          }

          if (!(TypeEquiv(*expected_key_type, *key_type_to_check))) {
            TypeMismatch(context, key_id, *expected_key_type,
                         *key_type_to_check);
            valid = false;
            break;
          }
          if (!(TypeEquiv(*expected_value_type, *val_type_to_check))) {
            TypeMismatch(context, val_id, *expected_value_type,
                         *val_type_to_check);
            valid = false;
            break;
          }
        }
      }
    }
    return valid;
  });
}

}  // namespace cel
