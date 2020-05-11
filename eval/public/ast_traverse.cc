// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stack>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/public/ast_traverse.h"
#include "eval/public/source_position.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;
using Ident = google::api::expr::v1alpha1::Expr::Ident;
using Select = google::api::expr::v1alpha1::Expr::Select;
using Call = google::api::expr::v1alpha1::Expr::Call;
using CreateList = google::api::expr::v1alpha1::Expr::CreateList;
using CreateStruct = google::api::expr::v1alpha1::Expr::CreateStruct;
using Comprehension = google::api::expr::v1alpha1::Expr::Comprehension;

namespace {

struct StackRecord {
 public:
  static constexpr int kNotCallArg = -1;

  StackRecord(const Expr *e, const SourceInfo *info)
      : expr(e),
        source_info(info),
        visited(false),
        calling_expr(nullptr),
        call_arg(kNotCallArg) {}

  StackRecord(const Expr *e, const SourceInfo *info, const Expr *call,
              int argnum)
      : expr(e),
        source_info(info),
        visited(false),
        calling_expr(call),
        call_arg(argnum) {}

  const Expr *expr;
  const SourceInfo *source_info;
  bool visited;

  // For records that are direct arguments to call, we need to call
  // the CallArg visitor immediately after the argument is evaluated.
  const Expr *calling_expr;
  int call_arg;
};

void PreVisit(const StackRecord &record, AstVisitor *visitor) {
  const Expr *expr = record.expr;
  const SourcePosition position(expr->id(), record.source_info);
  visitor->PreVisitExpr(expr, &position);
  switch (expr->expr_kind_case()) {
    case Expr::kSelectExpr:
      visitor->PreVisitSelect(&expr->select_expr(), expr, &position);
      break;
    case Expr::kCallExpr:
      visitor->PreVisitCall(&expr->call_expr(), expr, &position);
      break;
    case Expr::kComprehensionExpr:
      visitor->PreVisitComprehension(&expr->comprehension_expr(), expr,
                                     &position);
      break;
    default:
      // No pre-visit action.
      break;
  }
}

void PostVisit(const StackRecord &record, AstVisitor *visitor) {
  const Expr *expr = record.expr;
  const SourcePosition position(expr->id(), record.source_info);
  switch (expr->expr_kind_case()) {
    case Expr::kConstExpr:
      visitor->PostVisitConst(&expr->const_expr(), expr, &position);
      break;
    case Expr::kIdentExpr:
      visitor->PostVisitIdent(&expr->ident_expr(), expr, &position);
      break;
    case Expr::kSelectExpr:
      visitor->PostVisitSelect(&expr->select_expr(), expr, &position);
      break;
    case Expr::kCallExpr:
      visitor->PostVisitCall(&expr->call_expr(), expr, &position);
      break;
    case Expr::kListExpr:
      visitor->PostVisitCreateList(&expr->list_expr(), expr, &position);
      break;
    case Expr::kStructExpr:
      visitor->PostVisitCreateStruct(&expr->struct_expr(), expr, &position);
      break;
    case Expr::kComprehensionExpr:
      visitor->PostVisitComprehension(&expr->comprehension_expr(), expr,
                                      &position);
      break;
    default:
      GOOGLE_LOG(ERROR) << "Unsupported Expr kind: " << expr->expr_kind_case();
  }

  if (record.call_arg != StackRecord::kNotCallArg &&
      record.calling_expr != nullptr) {
    visitor->PostVisitArg(record.call_arg, record.calling_expr, &position);
  }
  visitor->PostVisitExpr(expr, &position);
}

void PushSelectDeps(const Select *select_expr, const StackRecord &record,
                    std::stack<StackRecord> *stack) {
  if (select_expr->has_operand()) {
    stack->push(StackRecord(&select_expr->operand(), record.source_info));
  }
}

void PushCallDeps(const Call *call_expr, const Expr *expr,
                  const StackRecord &record, std::stack<StackRecord> *stack) {
  const int arg_size = call_expr->args_size();
  // Our contract is that we visit arguments in order.  To do that, we need
  // to push them onto the stack in reverse order.
  for (int i = arg_size - 1; i >= 0; --i) {
    stack->push(StackRecord(&call_expr->args(i), record.source_info, expr, i));
  }
  // Are we receiver-style?
  if (call_expr->has_target()) {
    stack->push(StackRecord(&call_expr->target(), record.source_info));
  }
}

void PushListDeps(const CreateList *list_expr, const StackRecord &record,
                  std::stack<StackRecord> *stack) {
  const auto &elements = list_expr->elements();
  for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
    const auto &element = *it;
    stack->push(StackRecord(&element, record.source_info));
  }
}

void PushStructDeps(const CreateStruct *struct_expr, const StackRecord &record,
                    std::stack<StackRecord> *stack) {
  const auto &entries = struct_expr->entries();
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    const auto &entry = *it;
    // The contract is to visit key, then value.  So put them on the stack
    // in the opposite order.
    if (entry.has_value()) {
      stack->push(StackRecord(&entry.value(), record.source_info));
    }

    if (entry.has_map_key()) {
      stack->push(StackRecord(&entry.map_key(), record.source_info));
    }
  }
}

void PushComprehensionDeps(const Comprehension *c, const Expr *expr,
                           const StackRecord &record,
                           std::stack<StackRecord> *stack) {
  const SourceInfo *source_info = record.source_info;
  StackRecord iter_range(&c->iter_range(), source_info, expr, ITER_RANGE);
  StackRecord accu_init(&c->accu_init(), source_info, expr, ACCU_INIT);
  StackRecord loop_condition(&c->loop_condition(), source_info, expr,
                             LOOP_CONDITION);
  StackRecord loop_step(&c->loop_step(), source_info, expr, LOOP_STEP);
  StackRecord result(&c->result(), source_info, expr, RESULT);
  // Push them in reverse order.
  stack->push(result);
  stack->push(loop_step);
  stack->push(loop_condition);
  stack->push(accu_init);
  stack->push(iter_range);
}

void PushDependencies(const StackRecord &record,
                      std::stack<StackRecord> *stack) {
  const Expr *expr = record.expr;
  switch (expr->expr_kind_case()) {
    case Expr::kSelectExpr:
      PushSelectDeps(&expr->select_expr(), record, stack);
      break;
    case Expr::kCallExpr:
      PushCallDeps(&expr->call_expr(), expr, record, stack);
      break;
    case Expr::kListExpr:
      PushListDeps(&expr->list_expr(), record, stack);
      break;
    case Expr::kStructExpr:
      PushStructDeps(&expr->struct_expr(), record, stack);
      break;
    case Expr::kComprehensionExpr:
      PushComprehensionDeps(&expr->comprehension_expr(), expr, record, stack);
      break;
    default:
      break;
  }
}

}  // namespace

void AstTraverse(const Expr *expr, const SourceInfo *source_info,
                 AstVisitor *visitor) {
  std::stack<StackRecord> stack;
  stack.push(StackRecord(expr, source_info));

  while (!stack.empty()) {
    StackRecord &record = stack.top();
    if (!record.visited) {
      PreVisit(record, visitor);
      PushDependencies(record, &stack);
      record.visited = true;
    } else {
      PostVisit(record, visitor);
      stack.pop();
    }
  }
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
