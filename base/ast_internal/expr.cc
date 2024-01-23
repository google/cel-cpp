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

#include "base/ast_internal/expr.h"

#include <memory>
#include <stack>
#include <vector>

#include "absl/functional/overload.h"
#include "absl/types/variant.h"

namespace cel::ast_internal {

namespace {

const Expr& default_expr() {
  static Expr* expr = new Expr();
  return *expr;
}

const Type& default_type() {
  static Type* type = new Type();
  return *type;
}

struct CopyRecord {
  const Expr* src;
  Expr* dest;
};

void CopyNode(const Expr& src, std::stack<CopyRecord>& records, Expr& dest) {
  dest.set_id(src.id());

  const auto& src_kind = src.expr_kind();
  absl::visit(
      absl::Overload(
          [&](const Constant& constant) {
            dest.mutable_expr_kind() = constant;
          },
          [&](const Ident& ident) { dest.mutable_expr_kind() = ident; },
          [&](const Select& select) {
            auto& dest_select = dest.mutable_select_expr();
            dest_select.set_field(select.field());
            dest_select.set_test_only(select.test_only());
            records.push({&select.operand(), &dest_select.mutable_operand()});
          },
          [&](const Call& call) {
            auto& dest_call = dest.mutable_call_expr();
            dest_call.set_function(call.function());
            if (call.has_target()) {
              records.push({&call.target(), &dest_call.mutable_target()});
            }
            // pointer stability is guaranteed since the vector itself won't
            // change anywhere else in the copy routine.
            dest_call.mutable_args() = std::vector<Expr>(call.args().size());
            for (int i = 0; i < call.args().size(); ++i) {
              records.push({&call.args()[i], &dest_call.mutable_args()[i]});
            }
          },
          [&](const CreateList& create_list) {
            auto& dest_create_list = dest.mutable_list_expr();
            dest_create_list.optional_indices() =
                create_list.optional_indices();

            // pointer stability is guaranteed since the vector itself won't
            // change anywhere else in the copy routine.
            dest_create_list.mutable_elements() =
                std::vector<Expr>(create_list.elements().size());
            for (int i = 0; i < create_list.elements().size(); ++i) {
              records.push({&create_list.elements()[i],
                            &dest_create_list.mutable_elements()[i]});
            }
          },
          [&](const Comprehension& comprehension) {
            auto& dest_comprehension = dest.mutable_comprehension_expr();
            dest_comprehension.set_iter_var(comprehension.iter_var());
            dest_comprehension.set_accu_var(comprehension.accu_var());
            records.push({&comprehension.iter_range(),
                          &dest_comprehension.mutable_iter_range()});
            records.push({&comprehension.accu_init(),
                          &dest_comprehension.mutable_accu_init()});
            records.push({&comprehension.loop_condition(),
                          &dest_comprehension.mutable_loop_condition()});
            records.push({&comprehension.loop_step(),
                          &dest_comprehension.mutable_loop_step()});
            records.push({&comprehension.result(),
                          &dest_comprehension.mutable_result()});
          },
          [&](const CreateStruct& struct_expr) {
            auto& dest_struct_expr = dest.mutable_struct_expr();
            dest_struct_expr.set_message_name(struct_expr.message_name());

            dest_struct_expr.mutable_entries() =
                std::vector<CreateStruct::Entry>(struct_expr.entries().size());
            for (int i = 0; i < struct_expr.entries().size(); ++i) {
              auto& dest_entry = dest_struct_expr.mutable_entries()[i];
              const auto& entry = struct_expr.entries()[i];

              dest_entry.set_id(entry.id());
              dest_entry.set_optional_entry(entry.optional_entry());
              records.push({&entry.value(), &dest_entry.mutable_value()});

              if (entry.has_field_key()) {
                dest_entry.set_field_key(entry.field_key());
              } else {
                records.push({&entry.map_key(), &dest_entry.mutable_map_key()});
              }
            }
          },
          [&](absl::monostate) {
            // unset expr kind, nothing todo.
          }),
      src_kind);
}

TypeKind CopyImpl(const TypeKind& other) {
  return absl::visit(absl::Overload(
                         [](const std::unique_ptr<Type>& other) -> TypeKind {
                           return std::make_unique<Type>(*other);
                         },
                         [](const auto& other) -> TypeKind {
                           // Other variants define copy ctor.
                           return other;
                         }),
                     other);
}

}  // namespace

Expr Expr::DeepCopy() const {
  Expr copy;
  std::stack<CopyRecord> records;
  records.push(CopyRecord{this, &copy});
  while (!records.empty()) {
    CopyRecord next = records.top();
    records.pop();
    CopyNode(*next.src, records, *next.dest);
  }
  return copy;
}

SourceInfo SourceInfo::DeepCopy() const {
  SourceInfo copy;
  copy.location_ = location_;
  copy.syntax_version_ = syntax_version_;
  copy.line_offsets_ = line_offsets_;
  copy.positions_ = positions_;
  copy.macro_calls_.reserve(macro_calls_.size());
  for (auto it = macro_calls_.begin(); it != macro_calls_.end(); ++it) {
    copy.macro_calls_.insert_or_assign(it->first, it->second.DeepCopy());
  }
  return copy;
}

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
  return id_ == other.id_ && has_same_key && value() == other.value() &&
         optional_entry_ == other.optional_entry();
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

Type::Type(const Type& other) : type_kind_(CopyImpl(other.type_kind_)) {}

Type& Type::operator=(const Type& other) {
  type_kind_ = CopyImpl(other.type_kind_);
  return *this;
}

FunctionType::FunctionType(const FunctionType& other)
    : result_type_(std::make_unique<Type>(other.result_type())),
      arg_types_(other.arg_types()) {}

FunctionType& FunctionType::operator=(const FunctionType& other) {
  result_type_ = std::make_unique<Type>(other.result_type());
  arg_types_ = other.arg_types();
  return *this;
}

}  // namespace cel::ast_internal
