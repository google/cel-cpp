// Copyright 2022 Google LLC
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

#include "base/ast.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cel::ast::internal {

namespace {
const Expr& default_expr() {
  static Expr* expr = new Expr();
  return *expr;
}
}  // namespace

const Expr& Select::operand() const {
  if (operand_ != nullptr) {
    return *operand_;
  }
  return default_expr();
}

bool Select::operator==(const Select& other) const {
  return operand() == other.operand() && field_ == other.field_ &&
         test_only_ == other.test_only_;
}

const Expr& Call::target() const {
  if (target_ != nullptr) {
    return *target_;
  }
  return default_expr();
}

bool Call::operator==(const Call& other) const {
  return target() == other.target() && function_ == other.function_ &&
         args_ == other.args_;
}

const Expr& CreateStruct::Entry::map_key() const {
  auto* value = absl::get_if<std::unique_ptr<Expr>>(&key_kind_);
  if (value != nullptr) {
    if (*value != nullptr) return **value;
  }
  return default_expr();
}

const Expr& CreateStruct::Entry::value() const {
  if (value_ != nullptr) {
    return *value_;
  }
  return default_expr();
}

bool CreateStruct::Entry::operator==(const Entry& other) const {
  bool has_same_key = false;
  if (has_field_key() && other.has_field_key()) {
    has_same_key = field_key() == other.field_key();
  } else if (has_map_key() && other.has_map_key()) {
    has_same_key = map_key() == other.map_key();
  }
  return id_ == other.id_ && has_same_key && value() == other.value();
}

const Expr& Comprehension::iter_range() const {
  if (iter_range_ != nullptr) {
    return *iter_range_;
  }
  return default_expr();
}

const Expr& Comprehension::accu_init() const {
  if (accu_init_ != nullptr) {
    return *accu_init_;
  }
  return default_expr();
}

const Expr& Comprehension::loop_condition() const {
  if (loop_condition_ != nullptr) {
    return *loop_condition_;
  }
  return default_expr();
}

const Expr& Comprehension::loop_step() const {
  if (loop_step_ != nullptr) {
    return *loop_step_;
  }
  return default_expr();
}

const Expr& Comprehension::result() const {
  if (result_ != nullptr) {
    return *result_;
  }
  return default_expr();
}

bool Comprehension::operator==(const Comprehension& other) const {
  return iter_var_ == other.iter_var_ && iter_range() == other.iter_range() &&
         accu_var_ == other.accu_var_ && accu_init() == other.accu_init() &&
         loop_condition() == other.loop_condition() &&
         loop_step() == other.loop_step() && result() == other.result();
}

namespace {
const Type& default_type() {
  static Type* type = new Type();
  return *type;
}
}  // namespace

const Type& ListType::elem_type() const {
  if (elem_type_ != nullptr) {
    return *elem_type_;
  }
  return default_type();
}

bool ListType::operator==(const ListType& other) const {
  return elem_type() == other.elem_type();
}

const Type& MapType::key_type() const {
  if (key_type_ != nullptr) {
    return *key_type_;
  }
  return default_type();
}

const Type& MapType::value_type() const {
  if (value_type_ != nullptr) {
    return *value_type_;
  }
  return default_type();
}

bool MapType::operator==(const MapType& other) const {
  return key_type() == other.key_type() && value_type() == other.value_type();
}

const Type& FunctionType::result_type() const {
  if (result_type_ != nullptr) {
    return *result_type_;
  }
  return default_type();
}

bool FunctionType::operator==(const FunctionType& other) const {
  return result_type() == other.result_type() && arg_types_ == other.arg_types_;
}

const Type& Type::type() const {
  auto* value = absl::get_if<std::unique_ptr<Type>>(&type_kind_);
  if (value != nullptr) {
    if (*value != nullptr) return **value;
  }
  return default_type();
}

}  // namespace cel::ast::internal
