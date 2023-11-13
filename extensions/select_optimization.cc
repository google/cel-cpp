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

#include "extensions/select_optimization.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/attribute.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/types/struct_type.h"
#include "base/values/struct_value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/ast_rewrite_native.h"
#include "eval/public/source_position_native.h"
#include "internal/overloaded.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {
namespace {

using ::cel::ast_internal::AstImpl;
using ::cel::ast_internal::AstRewriterBase;
using ::cel::ast_internal::ConstantKind;
using ::cel::ast_internal::Expr;
using ::cel::ast_internal::ExprKind;
using ::cel::ast_internal::SourcePosition;
using ::google::api::expr::runtime::AttributeTrail;
using ::google::api::expr::runtime::ExecutionFrame;
using ::google::api::expr::runtime::ExpressionStepBase;
using ::google::api::expr::runtime::PlannerContext;
using ::google::api::expr::runtime::ProgramOptimizer;

// Represents a single select operation (field access or indexing).
// For struct-typed field accesses, includes the field name and the field
// number.
// TODO(uncreated-issue/51): initially, just covering the chained struct select case.
// Follow-ups will generalize for maps and lists.
struct SelectInstruction {
  int64_t number;
  absl::string_view name;
};

struct SelectPath {
  Expr* operand;
  std::vector<SelectInstruction> select_instructions;
  bool test_only;
  // TODO(uncreated-issue/54): support for optionals.
};

// Generates the AST representation of the qualification path for the optimized
// select branch. I.e., the list-typed second argument of the @cel.attribute
// call.
Expr MakeSelectPathExpr(
    const std::vector<SelectInstruction>& select_instructions) {
  Expr result;
  auto& list = result.mutable_list_expr().mutable_elements();
  list.reserve(select_instructions.size());
  for (const auto& instruction : select_instructions) {
    Expr ast_instruction;
    Expr field_number;
    field_number.mutable_const_expr().set_int64_value(instruction.number);
    Expr field_name;
    field_name.mutable_const_expr().set_string_value(
        std::string(instruction.name));
    auto& field_specifier =
        ast_instruction.mutable_list_expr().mutable_elements();
    field_specifier.push_back(std::move(field_number));
    field_specifier.push_back(std::move(field_name));

    list.push_back(std::move(ast_instruction));
  }
  return result;
}

// Returns a single select operation based on the inferred type of the operand
// and the field name. If the operand type doesn't define the field, returns
// nullopt.
absl::optional<SelectInstruction> GetSelectInstruction(
    const StructType& runtime_type, PlannerContext& planner_context,
    absl::string_view field_name) {
  auto field_or =
      runtime_type
          .FindFieldByName(planner_context.value_factory().type_manager(),
                           field_name)
          .value_or(absl::nullopt);
  if (field_or.has_value()) {
    return SelectInstruction{field_or->number, field_name};
  }
  return absl::nullopt;
}

absl::StatusOr<SelectQualifier> SelectInstructionFromList(
    const ast_internal::CreateList& list) {
  if (list.elements().size() != 2) {
    return absl::InvalidArgumentError("Invalid cel.attribute select list");
  }

  const Expr& field_number = list.elements()[0];
  const Expr& field_name = list.elements()[1];

  if (!field_number.has_const_expr() ||
      !field_number.const_expr().has_int64_value()) {
    return absl::InvalidArgumentError(
        "Invalid cel.attribute field select number");
  }

  if (!field_name.has_const_expr() ||
      !field_name.const_expr().has_string_value()) {
    return absl::InvalidArgumentError(
        "Invalid cel.attribute field select name");
  }

  return FieldSpecifier{field_number.const_expr().int64_value(),
                        field_name.const_expr().string_value()};
}

absl::StatusOr<std::vector<SelectQualifier>> SelectInstructionsFromCall(
    const ast_internal::Call& call) {
  if (call.args().size() < 2 || !call.args()[1].has_list_expr()) {
    return absl::InvalidArgumentError("Invalid cel.attribute call");
  }
  std::vector<SelectQualifier> instructions;
  const auto& ast_path = call.args()[1].list_expr().elements();
  instructions.reserve(ast_path.size());

  for (const Expr& element : ast_path) {
    // Optimized field select.
    if (element.has_list_expr()) {
      CEL_ASSIGN_OR_RETURN(instructions.emplace_back(),
                           SelectInstructionFromList(element.list_expr()));
    } else {
      return absl::UnimplementedError("map/list elements todo");
    }
  }

  // TODO(uncreated-issue/54): support for optionals.

  return instructions;
}

class RewriterImpl : public AstRewriterBase {
 public:
  RewriterImpl(const AstImpl& ast, PlannerContext& planner_context)
      : ast_(ast),
        planner_context_(planner_context),
        type_factory_(MemoryManagerRef::ReferenceCounting()) {}

  void PreVisitExpr(const Expr* expr, const SourcePosition* position) override {
    path_.push_back(expr);
    if (expr->has_select_expr()) {
      const Expr& operand = expr->select_expr().operand();
      const std::string& field_name = expr->select_expr().field();
      // Select optimization can generalize to lists and maps, but for now only
      // support message traversal.
      const ast_internal::Type& checker_type = ast_.GetType(operand.id());
      if (checker_type.has_message_type()) {
        auto type_or = GetRuntimeType(checker_type.message_type().type());
        if (type_or.has_value() && (*type_or)->Is<StructType>()) {
          const StructType& runtime_type = (*type_or)->As<StructType>();
          auto field_or =
              GetSelectInstruction(runtime_type, planner_context_, field_name);

          if (field_or.has_value()) {
            candidates_[expr] = std::move(field_or).value();
          }
        }
      }
    }
  }

  bool PostVisitRewrite(Expr* expr, const SourcePosition* position) override {
    path_.pop_back();
    auto candidate_iter = candidates_.find(expr);
    if (candidate_iter == candidates_.end()) {
      return false;
    }
    if (!path_.empty() && candidates_.find(path_.back()) != candidates_.end()) {
      // parent is optimizeable, defer rewriting until we consider the parent.
      return false;
    }

    SelectPath path = GetSelectPath(expr);

    // generate the new cel.attribute call.
    absl::string_view fn = path.test_only ? kFieldsHas : kCelAttribute;

    Expr operand(std::move(*path.operand));
    Expr call;
    call.set_id(expr->id());
    call.mutable_call_expr().set_function(std::string(fn));
    call.mutable_call_expr().mutable_args().reserve(2);

    call.mutable_call_expr().mutable_args().push_back(std::move(operand));
    call.mutable_call_expr().mutable_args().push_back(
        MakeSelectPathExpr(path.select_instructions));

    // TODO(uncreated-issue/54): support for optionals.

    *expr = std::move(call);

    return true;
  }

 private:
  SelectPath GetSelectPath(Expr* expr) {
    SelectPath result;
    result.test_only = false;
    Expr* operand = expr;
    auto candidate_iter = candidates_.find(operand);
    while (candidate_iter != candidates_.end()) {
      if (operand->select_expr().test_only()) {
        result.test_only = true;
      }
      result.select_instructions.push_back(candidate_iter->second);
      operand = &(operand->mutable_select_expr().mutable_operand());
      candidate_iter = candidates_.find(operand);
    }
    absl::c_reverse(result.select_instructions);
    result.operand = operand;
    return result;
  }

  absl::optional<Handle<Type>> GetRuntimeType(absl::string_view type_name) {
    return planner_context_.value_factory()
        .type_manager()
        .ResolveType(type_name)
        .value_or(absl::nullopt);
  }

  const AstImpl& ast_;
  PlannerContext& planner_context_;
  TypeFactory type_factory_;
  // ids of potentially optimizeable expr nodes.
  absl::flat_hash_map<const Expr*, SelectInstruction> candidates_;
  std::vector<const Expr*> path_;
};

class OptimizedSelectStep : public ExpressionStepBase {
 public:
  OptimizedSelectStep(int expr_id, absl::optional<Attribute> attribute,
                      std::vector<SelectQualifier> select_path,
                      std::vector<AttributeQualifier> qualifiers,
                      bool presence_test,
                      bool enable_wrapper_type_null_unboxing,
                      SelectOptimizationOptions options)
      : ExpressionStepBase(expr_id),
        attribute_(std::move(attribute)),
        select_path_(std::move(select_path)),
        qualifiers_(std::move(qualifiers)),
        presence_test_(presence_test),
        enable_wrapper_type_null_unboxing_(enable_wrapper_type_null_unboxing),
        options_(options)

  {
    ABSL_ASSERT(!select_path_.empty());
  }

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Handle<Value>> ApplySelect(
      ExecutionFrame* frame, const StructValue& struct_value) const;

  // Slow implementation if the Qualify operation isn't provided for the struct
  // implementation at runtime.
  absl::StatusOr<Handle<Value>> FallbackSelect(ExecutionFrame* frame,
                                               const StructValue& root) const;

  // Get the effective attribute for the optimized select expression.
  // Assumes the operand is the top of stack if the attribute wasn't known at
  // plan time.
  AttributeTrail GetAttributeTrail(ExecutionFrame* frame) const;

  absl::optional<Attribute> attribute_;
  std::vector<SelectQualifier> select_path_;
  std::vector<AttributeQualifier> qualifiers_;
  bool presence_test_;
  bool enable_wrapper_type_null_unboxing_;
  SelectOptimizationOptions options_;
};

// Check for unknowns or missing attributes.
absl::StatusOr<absl::optional<Handle<Value>>> CheckForMarkedAttributes(
    ExecutionFrame* frame, const AttributeTrail& attribute_trail) {
  if (attribute_trail.empty()) {
    return absl::nullopt;
  }

  if (frame->enable_unknowns()) {
    // Check if the inferred attribute is marked. Only matches if this attribute
    // or a parent is marked unknown (use_partial = false).
    // Partial matches (i.e. descendant of this attribute is marked) aren't
    // considered yet in case another operation would select an unmarked
    // descended attribute.
    //
    // TODO(uncreated-issue/51): this may return a more specific attribute than the
    // declared pattern. Follow up will truncate the returned attribute to match
    // the pattern.
    if (frame->attribute_utility().CheckForUnknown(attribute_trail,
                                                   /*use_partial=*/false)) {
      return frame->attribute_utility().CreateUnknownSet(
          attribute_trail.attribute());
    }
  }
  if (frame->enable_missing_attribute_errors()) {
    if (frame->attribute_utility().CheckForMissingAttribute(attribute_trail)) {
      return frame->attribute_utility().CreateMissingAttributeError(
          attribute_trail.attribute());
    }
  }

  return absl::nullopt;
}

AttributeTrail OptimizedSelectStep::GetAttributeTrail(
    ExecutionFrame* frame) const {
  if (attribute_.has_value()) {
    return AttributeTrail(*attribute_);
  }

  auto attr = frame->value_stack().PeekAttribute();
  std::vector<AttributeQualifier> qualifiers =
      attr.attribute().qualifier_path();
  qualifiers.reserve(qualifiers_.size() + qualifiers.size());
  absl::c_copy(qualifiers_, std::back_inserter(qualifiers));
  AttributeTrail result(Attribute(std::string(attr.attribute().variable_name()),
                                  std::move(qualifiers)));
  return result;
}

absl::StatusOr<Handle<Value>> OptimizedSelectStep::FallbackSelect(
    ExecutionFrame* frame, const StructValue& root) const {
  const StructValue* elem = &root;
  Handle<Value> result;

  for (const auto& instruction : select_path_) {
    if (elem == nullptr) {
      return absl::InvalidArgumentError("select path overflow");
    }
    if (!std::holds_alternative<FieldSpecifier>(instruction)) {
      return absl::UnimplementedError(
          "map and repeated field traversal not yet supported");
    }
    const auto& field_specifier = std::get<FieldSpecifier>(instruction);
    if (presence_test_) {
      // Check if any of the fields in the selection path is unset, returning
      // early if so.
      // If a parent message is unset, access is expected to return the
      // default instance (all fields defaulted and so not present
      // see
      // https://github.com/google/cel-spec/blob/master/doc/langdef.md#field-selection).
      CEL_ASSIGN_OR_RETURN(
          bool has_field,
          elem->HasFieldByName(frame->type_manager(), field_specifier.name));
      if (!has_field) {
        return frame->value_factory().CreateBoolValue(false);
      }
    }
    CEL_ASSIGN_OR_RETURN(result, elem->GetFieldByName(frame->value_factory(),
                                                      field_specifier.name));
    if (result->Is<StructValue>()) {
      elem = &result->As<StructValue>();
    }
  }
  if (presence_test_) {
    return frame->value_factory().CreateBoolValue(true);
  }
  return result;
}

absl::StatusOr<Handle<Value>> OptimizedSelectStep::ApplySelect(
    ExecutionFrame* frame, const StructValue& struct_value) const {
  absl::StatusOr<Handle<Value>> value_or =
      (options_.force_fallback_implementation)
          ? absl::UnimplementedError("Forced fallback impl")
          : struct_value.Qualify(frame->value_factory(), select_path_,
                                 presence_test_);

  if (value_or.status().code() != absl::StatusCode::kUnimplemented) {
    return value_or;
  }

  return FallbackSelect(frame, struct_value);
}

absl::Status OptimizedSelectStep::Evaluate(ExecutionFrame* frame) const {
  // Default empty.
  AttributeTrail attribute_trail;
  // TODO(uncreated-issue/51): add support for variable qualifiers and string literal
  // variable names.
  constexpr size_t kStackInputs = 1;

  if (frame->enable_attribute_tracking()) {
    // Compute the attribute trail then check for any marked values.
    // When possible, this is computed at plan time based on the optimized
    // select arguments.
    // TODO(uncreated-issue/51): add support variable qualifiers
    attribute_trail = GetAttributeTrail(frame);
    CEL_ASSIGN_OR_RETURN(absl::optional<Handle<Value>> value,
                         CheckForMarkedAttributes(frame, attribute_trail));
    if (value.has_value()) {
      frame->value_stack().Pop(kStackInputs);
      frame->value_stack().Push(std::move(value).value(),
                                std::move(attribute_trail));
      return absl::OkStatus();
    }
  }

  // Otherwise, we expect the operand to be top of stack.
  const Handle<Value>& operand = frame->value_stack().Peek();

  if (operand->Is<ErrorValue>() || operand->Is<UnknownValue>()) {
    // Just forward the error which is already top of stack.
    return absl::OkStatus();
  }

  if (!operand->Is<StructValue>()) {
    return absl::InvalidArgumentError(
        "Expected struct type for select optimization.");
  }

  CEL_ASSIGN_OR_RETURN(Handle<Value> result,
                       ApplySelect(frame, operand->As<StructValue>()));

  frame->value_stack().Pop(kStackInputs);
  frame->value_stack().Push(std::move(result), std::move(attribute_trail));
  return absl::OkStatus();
}

class SelectOptimizer : public ProgramOptimizer {
 public:
  explicit SelectOptimizer(const SelectOptimizationOptions& options)
      : options_(options) {}

  absl::Status OnPreVisit(PlannerContext& context,
                          const cel::ast_internal::Expr& node) override {
    return absl::OkStatus();
  }

  absl::Status OnPostVisit(PlannerContext& context,
                           const cel::ast_internal::Expr& node) override;

 private:
  SelectOptimizationOptions options_;
};

absl::Status SelectOptimizer::OnPostVisit(PlannerContext& context,
                                          const cel::ast_internal::Expr& node) {
  if (!node.has_call_expr()) {
    return absl::OkStatus();
  }

  absl::string_view fn = node.call_expr().function();
  if (fn != kFieldsHas && fn != kCelAttribute) {
    return absl::OkStatus();
  }

  if (node.call_expr().args().size() < 2 ||
      node.call_expr().args().size() > 3) {
    return absl::InvalidArgumentError("Invalid cel.attribute call");
  }

  if (node.call_expr().args().size() == 3) {
    return absl::UnimplementedError("Optionals not yet supported");
  }

  CEL_ASSIGN_OR_RETURN(std::vector<SelectQualifier> instructions,
                       SelectInstructionsFromCall(node.call_expr()));

  if (instructions.empty()) {
    return absl::InvalidArgumentError("Invalid cel.attribute no select steps.");
  }

  bool presence_test = false;

  if (fn == kFieldsHas) {
    presence_test = true;
  }

  const Expr& operand = node.call_expr().args()[0];
  absl::optional<Attribute> attribute;
  absl::string_view identifier;
  if (operand.has_ident_expr()) {
    identifier = operand.ident_expr().name();
  }

  if (absl::StrContains(identifier, ".")) {
    return absl::UnimplementedError("qualified identifiers not supported.");
  }

  std::vector<AttributeQualifier> qualifiers;
  qualifiers.reserve(instructions.size());
  for (const auto& instruction : instructions) {
    qualifiers.push_back(absl::visit(
        internal::Overloaded{[](const FieldSpecifier& field) {
                               return AttributeQualifier::OfString(field.name);
                             },
                             [](const AttributeQualifier& q) { return q; }},
        instruction));
  }

  if (!identifier.empty()) {
    attribute.emplace(std::string(identifier), qualifiers);
  }

  // TODO(uncreated-issue/51): If the first argument is a string literal, the custom
  // step needs to handle variable lookup.
  google::api::expr::runtime::ExecutionPath path;

  // else, we need to preserve the original plan for the first argument.
  if (context.GetSubplan(operand).empty()) {
    // Indicates another extension modified the step. Nothing to do here.
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto operand_subplan, context.ExtractSubplan(operand));
  absl::c_move(operand_subplan, std::back_inserter(path));

  bool enable_wrapper_type_null_unboxing =
      context.options().enable_empty_wrapper_null_unboxing;
  path.push_back(std::make_unique<OptimizedSelectStep>(
      node.id(), std::move(attribute), std::move(instructions),
      std::move(qualifiers), presence_test, enable_wrapper_type_null_unboxing,
      options_));

  return context.ReplaceSubplan(node, std::move(path));
}

}  // namespace

absl::Status SelectOptimizationAstUpdater::UpdateAst(PlannerContext& context,
                                                     AstImpl& ast) const {
  RewriterImpl rewriter(ast, context);

  ast_internal::AstRewrite(&ast.root_expr(), &ast.source_info(), &rewriter);
  return absl::OkStatus();
}

google::api::expr::runtime::ProgramOptimizerFactory
CreateSelectOptimizationProgramOptimizer(
    const SelectOptimizationOptions& options) {
  return [=](PlannerContext& context, const cel::ast_internal::AstImpl& ast) {
    return std::make_unique<SelectOptimizer>(options);
  };
}

}  // namespace cel::extensions
