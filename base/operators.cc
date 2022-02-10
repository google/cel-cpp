// Copyright 2021 Google LLC
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

#include "base/operators.h"

#include <memory>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

// Macro definining all the operators and their properties.
// (1) - The identifier.
// (2) - The display name if applicable, otherwise an empty string.
// (3) - The name.
// (4) - The precedence if applicable, otherwise 0.
// (5) - The arity.
#define CEL_OPERATORS_ENUM(XX)                          \
  XX(Conditional, "", "_?_:_", 8, 3)                    \
  XX(LogicalOr, "||", "_||_", 7, 2)                     \
  XX(LogicalAnd, "&&", "_&&_", 6, 2)                    \
  XX(Equals, "==", "_==_", 5, 2)                        \
  XX(NotEquals, "!=", "_!=_", 5, 2)                     \
  XX(Less, "<", "_<_", 5, 2)                            \
  XX(LessEquals, "<=", "_<=_", 5, 2)                    \
  XX(Greater, ">", "_>_", 5, 2)                         \
  XX(GreaterEquals, ">=", "_>=_", 5, 2)                 \
  XX(In, "in", "@in", 5, 2)                             \
  XX(OldIn, "in", "_in_", 5, 2)                         \
  XX(Add, "+", "_+_", 4, 2)                             \
  XX(Subtract, "-", "_-_", 4, 2)                        \
  XX(Multiply, "*", "_*_", 3, 2)                        \
  XX(Divide, "/", "_/_", 3, 2)                          \
  XX(Modulo, "%", "_%_", 3, 2)                          \
  XX(LogicalNot, "!", "!_", 2, 1)                       \
  XX(Negate, "-", "-_", 2, 1)                           \
  XX(Index, "", "_[_]", 1, 2)                           \
  XX(NotStrictlyFalse, "", "@not_strictly_false", 0, 1) \
  XX(OldNotStrictlyFalse, "", "__not_strictly_false__", 0, 1)

namespace cel {

namespace {

ABSL_CONST_INIT absl::once_flag operators_once_flag;
ABSL_CONST_INIT const absl::flat_hash_map<absl::string_view, Operator>*
    operators_by_name = nullptr;
ABSL_CONST_INIT const absl::flat_hash_map<absl::string_view, Operator>*
    operators_by_display_name = nullptr;
ABSL_CONST_INIT const absl::flat_hash_map<absl::string_view, Operator>*
    unary_operators = nullptr;
ABSL_CONST_INIT const absl::flat_hash_map<absl::string_view, Operator>*
    binary_operators = nullptr;

void InitializeOperators() {
  ABSL_ASSERT(operators_by_name == nullptr);
  ABSL_ASSERT(operators_by_display_name == nullptr);
  ABSL_ASSERT(unary_operators == nullptr);
  ABSL_ASSERT(binary_operators == nullptr);
  auto operators_by_name_ptr =
      std::make_unique<absl::flat_hash_map<absl::string_view, Operator>>();
  auto operators_by_display_name_ptr =
      std::make_unique<absl::flat_hash_map<absl::string_view, Operator>>();
  auto unary_operators_ptr =
      std::make_unique<absl::flat_hash_map<absl::string_view, Operator>>();
  auto binary_operators_ptr =
      std::make_unique<absl::flat_hash_map<absl::string_view, Operator>>();

#define CEL_DEFINE_OPERATORS_BY_NAME(id, symbol, name, precedence, arity) \
  if constexpr (!absl::string_view(name).empty()) {                       \
    operators_by_name_ptr->insert({name, Operator::id()});                \
  }
  CEL_OPERATORS_ENUM(CEL_DEFINE_OPERATORS_BY_NAME)
#undef CEL_DEFINE_OPERATORS_BY_NAME

#define CEL_DEFINE_OPERATORS_BY_SYMBOL(id, symbol, name, precedence, arity) \
  if constexpr (!absl::string_view(symbol).empty()) {                       \
    operators_by_display_name_ptr->insert({symbol, Operator::id()});        \
  }
  CEL_OPERATORS_ENUM(CEL_DEFINE_OPERATORS_BY_SYMBOL)
#undef CEL_DEFINE_OPERATORS_BY_SYMBOL

#define CEL_DEFINE_UNARY_OPERATORS(id, symbol, name, precedence, arity) \
  if constexpr (!absl::string_view(symbol).empty() && arity == 1) {     \
    unary_operators_ptr->insert({symbol, Operator::id()});              \
  }
  CEL_OPERATORS_ENUM(CEL_DEFINE_UNARY_OPERATORS)
#undef CEL_DEFINE_UNARY_OPERATORS

#define CEL_DEFINE_BINARY_OPERATORS(id, symbol, name, precedence, arity) \
  if constexpr (!absl::string_view(symbol).empty() && arity == 2) {      \
    binary_operators_ptr->insert({symbol, Operator::id()});              \
  }
  CEL_OPERATORS_ENUM(CEL_DEFINE_BINARY_OPERATORS)
#undef CEL_DEFINE_BINARY_OPERATORS

  operators_by_name = operators_by_name_ptr.release();
  operators_by_display_name = operators_by_display_name_ptr.release();
  unary_operators = unary_operators_ptr.release();
  binary_operators = binary_operators_ptr.release();
}

#define CEL_DEFINE_OPERATOR_DATA(id, symbol, name, precedence, arity) \
  ABSL_CONST_INIT constexpr base_internal::OperatorData k##id##Data(  \
      OperatorId::k##id, name, symbol, precedence, arity);
CEL_OPERATORS_ENUM(CEL_DEFINE_OPERATOR_DATA)
#undef CEL_DEFINE_OPERATOR_DATA

}  // namespace

#define CEL_DEFINE_OPERATOR(id, symbol, name, precedence, arity) \
  Operator Operator::id() { return Operator(std::addressof(k##id##Data)); }
CEL_OPERATORS_ENUM(CEL_DEFINE_OPERATOR)
#undef CEL_DEFINE_OPERATOR

absl::StatusOr<Operator> Operator::FindByName(absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  auto it = operators_by_name->find(input);
  if (it != operators_by_name->end()) {
    return it->second;
  }
  return absl::NotFoundError(absl::StrCat("No such operator: ", input));
}

absl::StatusOr<Operator> Operator::FindByDisplayName(absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  auto it = operators_by_display_name->find(input);
  if (it != operators_by_display_name->end()) {
    return it->second;
  }
  return absl::NotFoundError(absl::StrCat("No such operator: ", input));
}

absl::StatusOr<Operator> Operator::FindUnaryByDisplayName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  auto it = unary_operators->find(input);
  if (it != unary_operators->end()) {
    return it->second;
  }
  return absl::NotFoundError(absl::StrCat("No such unary operator: ", input));
}

absl::StatusOr<Operator> Operator::FindBinaryByDisplayName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  auto it = binary_operators->find(input);
  if (it != binary_operators->end()) {
    return it->second;
  }
  return absl::NotFoundError(absl::StrCat("No such binary operator: ", input));
}

}  // namespace cel

#undef CEL_OPERATORS_ENUM
