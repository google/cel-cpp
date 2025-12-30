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

#include "common/expr.h"

#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/types/variant.h"
#include "common/constant.h"

namespace cel {

namespace {

struct CopyStackRecord {
  const Expr* src;
  Expr* dst;
};

void CopyNode(CopyStackRecord element, std::vector<CopyStackRecord>& stack) {
  const Expr* src = element.src;
  Expr* dst = element.dst;
  dst->set_id(src->id());
  absl::visit(
      absl::Overload(
          [=](const UnspecifiedExpr&) {
            dst->mutable_kind().emplace<UnspecifiedExpr>();
          },
          [=](const IdentExpr& i) {
            dst->mutable_ident_expr().set_name(i.name());
          },
          [=](const Constant& c) { dst->mutable_const_expr() = c; },
          [&](const SelectExpr& s) {
            dst->mutable_select_expr().set_field(s.field());
            dst->mutable_select_expr().set_test_only(s.test_only());

            if (s.has_operand()) {
              stack.push_back({&s.operand(),
                               &dst->mutable_select_expr().mutable_operand()});
            }
          },
          [&](const CallExpr& c) {
            dst->mutable_call_expr().set_function(c.function());
            if (c.has_target()) {
              stack.push_back(
                  {&c.target(), &dst->mutable_call_expr().mutable_target()});
            }
            dst->mutable_call_expr().mutable_args().resize(c.args().size());
            for (int i = 0; i < dst->mutable_call_expr().mutable_args().size();
                 ++i) {
              stack.push_back(
                  {&c.args()[i], &dst->mutable_call_expr().mutable_args()[i]});
            }
          },
          [&](const ListExpr& c) {
            auto& dst_list = dst->mutable_list_expr();
            dst_list.mutable_elements().resize(c.elements().size());
            for (int i = 0; i < src->list_expr().elements().size(); ++i) {
              dst_list.mutable_elements()[i].set_optional(
                  c.elements()[i].optional());
              stack.push_back({&c.elements()[i].expr(),
                               &dst_list.mutable_elements()[i].mutable_expr()});
            }
          },
          [&](const StructExpr& s) {
            auto& dst_struct = dst->mutable_struct_expr();
            dst_struct.mutable_fields().resize(s.fields().size());
            dst_struct.set_name(s.name());
            for (int i = 0; i < s.fields().size(); ++i) {
              dst_struct.mutable_fields()[i].set_optional(
                  s.fields()[i].optional());
              dst_struct.mutable_fields()[i].set_name(s.fields()[i].name());
              dst_struct.mutable_fields()[i].set_id(s.fields()[i].id());
              stack.push_back(
                  {&s.fields()[i].value(),
                   &dst_struct.mutable_fields()[i].mutable_value()});
            }
          },
          [&](const MapExpr& c) {
            auto& dst_map = dst->mutable_map_expr();
            dst_map.mutable_entries().resize(c.entries().size());
            for (int i = 0; i < c.entries().size(); ++i) {
              dst_map.mutable_entries()[i].set_optional(
                  c.entries()[i].optional());
              dst_map.mutable_entries()[i].set_id(c.entries()[i].id());
              stack.push_back({&c.entries()[i].key(),
                               &dst_map.mutable_entries()[i].mutable_key()});
              stack.push_back({&c.entries()[i].value(),
                               &dst_map.mutable_entries()[i].mutable_value()});
            }
          },
          [&](const ComprehensionExpr& c) {
            auto& dst_comprehension = dst->mutable_comprehension_expr();
            dst_comprehension.set_iter_var(c.iter_var());
            dst_comprehension.set_iter_var2(c.iter_var2());
            dst_comprehension.set_accu_var(c.accu_var());
            if (c.has_accu_init()) {
              stack.push_back(
                  {&c.accu_init(), &dst_comprehension.mutable_accu_init()});
            }
            if (c.has_iter_range()) {
              stack.push_back(
                  {&c.iter_range(), &dst_comprehension.mutable_iter_range()});
            }
            if (c.has_loop_condition()) {
              stack.push_back({&c.loop_condition(),
                               &dst_comprehension.mutable_loop_condition()});
            }
            if (c.has_loop_step()) {
              stack.push_back(
                  {&c.loop_step(), &dst_comprehension.mutable_loop_step()});
            }
            if (c.has_result()) {
              stack.push_back(
                  {&c.result(), &dst_comprehension.mutable_result()});
            }
          }),
      src->kind());
}

void CloneImpl(const Expr& expr, Expr& dst) {
  std::vector<CopyStackRecord> stack;
  stack.push_back({&expr, &dst});
  while (!stack.empty()) {
    CopyStackRecord element = stack.back();
    stack.pop_back();
    CopyNode(element, stack);
  }
}

}  // namespace

const UnspecifiedExpr& UnspecifiedExpr::default_instance() {
  static const absl::NoDestructor<UnspecifiedExpr> instance;
  return *instance;
}

const IdentExpr& IdentExpr::default_instance() {
  static const absl::NoDestructor<IdentExpr> instance;
  return *instance;
}

const SelectExpr& SelectExpr::default_instance() {
  static const absl::NoDestructor<SelectExpr> instance;
  return *instance;
}

const CallExpr& CallExpr::default_instance() {
  static const absl::NoDestructor<CallExpr> instance;
  return *instance;
}

const ListExpr& ListExpr::default_instance() {
  static const absl::NoDestructor<ListExpr> instance;
  return *instance;
}

const StructExpr& StructExpr::default_instance() {
  static const absl::NoDestructor<StructExpr> instance;
  return *instance;
}

const MapExpr& MapExpr::default_instance() {
  static const absl::NoDestructor<MapExpr> instance;
  return *instance;
}

const ComprehensionExpr& ComprehensionExpr::default_instance() {
  static const absl::NoDestructor<ComprehensionExpr> instance;
  return *instance;
}

const Expr& Expr::default_instance() {
  static const absl::NoDestructor<Expr> instance;
  return *instance;
}

Expr& Expr::operator=(const Expr& other) {
  if (this == &other) {
    return *this;
  }
  Expr tmp;
  CloneImpl(other, tmp);
  *this = std::move(tmp);
  return *this;
}

Expr::Expr(const Expr& other) { CloneImpl(other, *this); }

namespace common_internal {
struct ExprEraseTag {};
}  // namespace common_internal

namespace {
void Expand(Expr** tail, Expr* cur) {
  static common_internal::ExprEraseTag tag;
  switch (cur->kind_case()) {
    case ExprKindCase::kSelectExpr: {
      SelectExpr& select = cur->mutable_select_expr();
      if (select.has_operand()) {
        select.mutable_operand().SetNext(tag, *tail);
        *tail = &select.mutable_operand();
      }
      break;
    }
    case ExprKindCase::kCallExpr: {
      CallExpr& call = cur->mutable_call_expr();
      if (call.has_target()) {
        call.mutable_target().SetNext(tag, *tail);
        *tail = &call.mutable_target();
      }
      for (auto& arg : call.mutable_args()) {
        arg.SetNext(tag, *tail);
        *tail = &arg;
      }
      break;
    }
    case ExprKindCase::kListExpr: {
      for (auto& arg : cur->mutable_list_expr().mutable_elements()) {
        arg.mutable_expr().SetNext(tag, *tail);
        *tail = &arg.mutable_expr();
      }
      break;
    }
    case ExprKindCase::kStructExpr: {
      for (auto& field : cur->mutable_struct_expr().mutable_fields()) {
        field.mutable_value().SetNext(tag, *tail);
        *tail = &field.mutable_value();
      }
      break;
    }
    case ExprKindCase::kMapExpr: {
      for (auto& entry : cur->mutable_map_expr().mutable_entries()) {
        entry.mutable_key().SetNext(tag, *tail);
        *tail = &entry.mutable_key();
        entry.mutable_value().SetNext(tag, *tail);
        *tail = &entry.mutable_value();
      }
      break;
    }
    case ExprKindCase::kComprehensionExpr: {
      if (cur->comprehension_expr().has_accu_init()) {
        cur->mutable_comprehension_expr().mutable_accu_init().SetNext(tag,
                                                                      *tail);
        *tail = &cur->mutable_comprehension_expr().mutable_accu_init();
      }
      if (cur->comprehension_expr().has_iter_range()) {
        cur->mutable_comprehension_expr().mutable_iter_range().SetNext(tag,
                                                                       *tail);
        *tail = &cur->mutable_comprehension_expr().mutable_iter_range();
      }
      if (cur->comprehension_expr().has_loop_condition()) {
        cur->mutable_comprehension_expr().mutable_loop_condition().SetNext(
            tag, *tail);
        *tail = &cur->mutable_comprehension_expr().mutable_loop_condition();
      }
      if (cur->comprehension_expr().has_loop_step()) {
        cur->mutable_comprehension_expr().mutable_loop_step().SetNext(tag,
                                                                      *tail);
        *tail = &cur->mutable_comprehension_expr().mutable_loop_step();
      }
      if (cur->comprehension_expr().has_result()) {
        cur->mutable_comprehension_expr().mutable_result().SetNext(tag, *tail);
        *tail = &cur->mutable_comprehension_expr().mutable_result();
      }
      break;
    }
    default:
      // Leaf node, nothing to expand.
      // Also a fallback in case we add a new node type.
      // Note: already in the deleter list so we can't delete now, will be
      // deleted after ordering the AST.
      break;
  }
}
}  // namespace

void Expr::FlattenedErase() {
  // High level idea is to build a topological ordering of the AST, then erase
  // leaf to root.
  this->u_.next = nullptr;
  Expr* prev_tail = nullptr;
  Expr* tail = this;

  while (tail != prev_tail) {
    Expr* next_prev_tail = tail;
    Expr* expand_ptr = tail;
    while (expand_ptr != prev_tail) {
      ABSL_DCHECK(expand_ptr != nullptr);  // Linked list is broken or changed.
      Expr* next_expand_ptr = expand_ptr->u_.next;
      Expand(&tail, expand_ptr);
      expand_ptr = next_expand_ptr;
    }
    prev_tail = next_prev_tail;
  }

  Expr* node = tail;
  while (node != nullptr) {
    Expr* next = node->u_.next;
    node->Clear();
    node = next;
  }
}

}  // namespace cel
