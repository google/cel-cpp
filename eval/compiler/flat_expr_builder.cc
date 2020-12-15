#include "eval/compiler/flat_expr_builder.h"

#include <memory>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "stack"
#include "absl/container/node_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/qualified_reference_resolver.h"
#include "eval/eval/comprehension_step.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/container_access_step.h"
#include "eval/eval/create_list_step.h"
#include "eval/eval/create_struct_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/eval/function_step.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/jump_step.h"
#include "eval/eval/logic_step.h"
#include "eval/eval/select_step.h"
#include "eval/eval/ternary_step.h"
#include "eval/public/ast_traverse.h"
#include "eval/public/ast_visitor.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::api::expr::v1alpha1::CheckedExpr;
using google::api::expr::v1alpha1::Constant;
using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::Reference;
using google::api::expr::v1alpha1::SourceInfo;
using Ident = google::api::expr::v1alpha1::Expr::Ident;
using Select = google::api::expr::v1alpha1::Expr::Select;
using Call = google::api::expr::v1alpha1::Expr::Call;
using CreateList = google::api::expr::v1alpha1::Expr::CreateList;
using CreateStruct = google::api::expr::v1alpha1::Expr::CreateStruct;
using Comprehension = google::api::expr::v1alpha1::Expr::Comprehension;

// Forward declare to resolve circular dependency for short_circuiting visitors.
class FlatExprVisitor;

// A convenience wrapper for offset-calculating logic.
class Jump {
 public:
  explicit Jump() : self_index_(-1), jump_step_(nullptr) {}
  explicit Jump(int self_index, JumpStepBase* jump_step)
      : self_index_(self_index), jump_step_(jump_step) {}
  void set_target(int index) {
    // 0 offset means no-op.
    jump_step_->set_jump_offset(index - self_index_ - 1);
  }
  bool exists() { return jump_step_ != nullptr; }

 private:
  int self_index_;
  JumpStepBase* jump_step_;
};

class CondVisitor {
 public:
  virtual ~CondVisitor() {}
  virtual void PreVisit(const Expr* expr) = 0;
  virtual void PostVisitArg(int arg_num, const Expr* expr) = 0;
  virtual void PostVisit(const Expr* expr) = 0;
};

// Visitor managing the "&&" and "||" operatiions.
class BinaryCondVisitor : public CondVisitor {
 public:
  explicit BinaryCondVisitor(FlatExprVisitor* visitor, bool cond_value,
                             bool short_circuiting)
      : visitor_(visitor),
        cond_value_(cond_value),
        short_circuiting_(short_circuiting) {}

  void PreVisit(const Expr* expr) override;
  void PostVisitArg(int arg_num, const Expr* expr) override;
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  const bool cond_value_;
  Jump jump_step_;
  bool short_circuiting_;
};

class TernaryCondVisitor : public CondVisitor {
 public:
  explicit TernaryCondVisitor(FlatExprVisitor* visitor) : visitor_(visitor) {}

  void PreVisit(const Expr* expr) override;
  void PostVisitArg(int arg_num, const Expr* expr) override;
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  Jump jump_to_second_;
  Jump error_jump_;
  Jump jump_after_first_;
};

class ExhaustiveTernaryCondVisitor : public CondVisitor {
 public:
  explicit ExhaustiveTernaryCondVisitor(FlatExprVisitor* visitor)
      : visitor_(visitor) {}

  void PreVisit(const Expr* expr) override {}
  void PostVisitArg(int arg_num, const Expr* expr) override {}
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
};

// Visitor Comprehension expression.
class ComprehensionVisitor : public CondVisitor {
 public:
  explicit ComprehensionVisitor(FlatExprVisitor* visitor, bool short_circuiting)
      : visitor_(visitor),
        next_step_(nullptr),
        cond_step_(nullptr),
        short_circuiting_(short_circuiting) {}

  void PreVisit(const Expr* expr) override;
  void PostVisitArg(int arg_num, const Expr* expr) override;
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  ComprehensionNextStep* next_step_;
  ComprehensionCondStep* cond_step_;
  int next_step_pos_;
  int cond_step_pos_;
  bool short_circuiting_;
};

class FlatExprVisitor : public AstVisitor {
 public:
  FlatExprVisitor(
      const CelFunctionRegistry* function_registry, ExecutionPath* path,
      bool short_circuiting,
      const std::set<const google::protobuf::EnumDescriptor*>& enums,
      absl::string_view container,
      const absl::flat_hash_map<std::string, CelValue>& constant_idents,
      bool enable_comprehension, BuilderWarnings* warnings,
      std::set<std::string>* iter_variable_names)
      : flattened_path_(path),
        progress_status_(absl::OkStatus()),
        resolved_select_expr_(nullptr),
        function_registry_(function_registry),
        short_circuiting_(short_circuiting),
        constant_idents_(constant_idents),
        enable_comprehension_(enable_comprehension),
        builder_warnings_(warnings),
        iter_variable_names_(iter_variable_names) {
    GOOGLE_CHECK(iter_variable_names_);

    auto container_elements = absl::StrSplit(container, '.');

    // Build list of prefixes from container. Non-empty prefixes must end with
    // ".", otherwise prefix "abc.xy" will match "abc.xyz.EnumName".
    std::string prefix = "";
    std::vector<std::string> prefixes;
    prefixes.push_back(prefix);
    for (const auto& elem : container_elements) {
      absl::StrAppend(&prefix, elem, ".");
      prefixes.push_back(prefix);
    }

    for (const auto& prefix : prefixes) {
      for (auto enum_desc : enums) {
        absl::string_view enum_name = enum_desc->full_name();
        if (!absl::StartsWith(enum_name, prefix)) {
          continue;
        }

        auto remainder = absl::StripPrefix(enum_name, prefix);
        for (int i = 0; i < enum_desc->value_count(); i++) {
          auto value_desc = enum_desc->value(i);
          if (value_desc) {
            // "prefixes" container is ascending-ordered. As such, we will be
            // assigning enum reference to the deepest available.
            // E.g. if both a.b.c.Name and a.b.Name are available, and
            // we try to reference "Name" with the scope of "a.b.c",
            // it will be resolved to "a.b.c.Name".
            auto key = absl::StrCat(remainder, !remainder.empty() ? "." : "",
                                    value_desc->name());
            enum_map_[key] = value_desc;
          }
        }
      }
    }
  }

  void PostVisitConst(const Constant* const_expr, const Expr* expr,
                      const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    auto value = ConvertConstant(const_expr);
    if (value.has_value()) {
      AddStep(CreateConstValueStep(value.value(), expr->id()));
    } else {
      SetProgressStatusError(absl::Status(absl::StatusCode::kInvalidArgument,
                                          "Unsupported constant type"));
    }
  }

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const Ident* ident_expr, const Expr* expr,
                      const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    std::string path(ident_expr->name());

    // Automatically replace constant idents with the backing CEL values.
    auto constant = constant_idents_.find(path);
    if (constant != constant_idents_.end()) {
      AddStep(CreateConstValueStep(constant->second, expr->id(), false));
      return;
    }

    // Attempt to resolve the identifier as an enum.
    const google::protobuf::EnumValueDescriptor* value_desc = nullptr;
    auto it = enum_map_.find(path);
    if (it != enum_map_.end()) {
      value_desc = it->second;
    }

    // Attempt to resolve a parsed-only expression as a namespaced identifier.
    while (!namespace_stack_.empty()) {
      const auto& select_node = namespace_stack_.back();
      // Generate path in format "<ident>.<field 0>.<field 1>...".
      absl::StrAppend(&path, ".", select_node.second);
      namespace_map_[select_node.first] = path;

      // Attempt to match namespace
      auto it = enum_map_.find(path);
      if (it != enum_map_.end()) {
        resolved_select_expr_ = select_node.first;
        value_desc = it->second;
      }
      namespace_stack_.pop_back();
    }

    if (resolved_select_expr_) {
      if (!resolved_select_expr_->has_select_expr()) {
        progress_status_ = absl::InternalError("Unexpected Expr type");
        return;
      }
      AddStep(CreateConstValueStep(value_desc, resolved_select_expr_->id()));
      return;
    }
    if (value_desc) {
      AddStep(CreateConstValueStep(value_desc, expr->id()));
      return;
    }
    AddStep(CreateIdentStep(ident_expr, expr->id()));
  }

  void PreVisitSelect(const Select* select_expr, const Expr* expr,
                      const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    // Not exactly the cleanest solution - we peek into child of
    // select_expr.
    // Chain of multiple SELECT ending with IDENT can represent namespaced
    // entity.
    if (select_expr->operand().has_ident_expr() ||
        select_expr->operand().has_select_expr()) {
      namespace_stack_.push_back({expr, select_expr->field()});
    } else {
      namespace_stack_.clear();
    }
  }

  // Select node handler.
  // Invoked after child nodes are processed.
  void PostVisitSelect(const Select* select_expr, const Expr* expr,
                       const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    // Check if we are "in the middle" of namespaced name.
    // This is currently enum specific. Constant expression that corresponds
    // to resolved enum value has been already created, thus preceding chain
    // of selects is no longer relevant.
    if (resolved_select_expr_) {
      if (expr == resolved_select_expr_) {
        resolved_select_expr_ = nullptr;
      }
      return;
    }

    std::string select_path = "";

    auto it = namespace_map_.find(expr);
    if (it != namespace_map_.end()) {
      select_path = it->second;
    }

    AddStep(CreateSelectStep(select_expr, expr->id(), select_path));
  }

  // Call node handler group.
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const Call* call_expr, const Expr* expr,
                    const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    std::unique_ptr<CondVisitor> cond_visitor;
    if (call_expr->function() == builtin::kAnd) {
      cond_visitor = absl::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ false, short_circuiting_);
    } else if (call_expr->function() == builtin::kOr) {
      cond_visitor = absl::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ true, short_circuiting_);
    } else if (call_expr->function() == builtin::kTernary) {
      if (short_circuiting_) {
        cond_visitor = absl::make_unique<TernaryCondVisitor>(this);
      } else {
        cond_visitor = absl::make_unique<ExhaustiveTernaryCondVisitor>(this);
      }
    } else {
      return;
    }

    if (cond_visitor) {
      cond_visitor->PreVisit(expr);
      cond_visitor_stack_.emplace(expr, std::move(cond_visitor));
    }
  }

  // Invoked after all child nodes are processed.
  void PostVisitCall(const Call* call_expr, const Expr* expr,
                     const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    auto cond_visitor = FindCondVisitor(expr);
    if (cond_visitor) {
      cond_visitor->PostVisit(expr);
      cond_visitor_stack_.pop();
    } else {
      // Special case for "_[_]".
      if (call_expr->function() == builtin::kIndex) {
        AddStep(CreateContainerAccessStep(call_expr, expr->id()));
        return;
      }
      // For regular functions, just create one based on registry.
      AddStep(CreateFunctionStep(call_expr, expr->id(), *function_registry_,
                                 builder_warnings_));
    }
  }

  void PreVisitComprehension(const Comprehension*, const Expr* expr,
                             const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!enable_comprehension_) {
      SetProgressStatusError(absl::Status(absl::StatusCode::kInvalidArgument,
                                          "Comprehension support is disabled"));
    }
    cond_visitor_stack_.emplace(
        expr, absl::make_unique<ComprehensionVisitor>(this, short_circuiting_));
    auto cond_visitor = FindCondVisitor(expr);
    cond_visitor->PreVisit(expr);
  }

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(const Comprehension* comprehension_expr,
                              const Expr* expr,
                              const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    auto cond_visitor = FindCondVisitor(expr);
    cond_visitor->PostVisit(expr);
    cond_visitor_stack_.pop();

    // Save off the names of the variables we're using, such that we have a
    // full set of the names from the entire evaluation tree at the end.
    if (!comprehension_expr->accu_var().empty()) {
      iter_variable_names_->insert(comprehension_expr->accu_var());
    }
    if (!comprehension_expr->iter_var().empty()) {
      iter_variable_names_->insert(comprehension_expr->iter_var());
    }
  }

  // Invoked after each argument node processed.
  void PostVisitArg(int arg_num, const Expr* expr,
                    const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    auto cond_visitor = FindCondVisitor(expr);
    if (cond_visitor) {
      cond_visitor->PostVisitArg(arg_num, expr);
    }
  }

  // Nothing to do.
  void PostVisitTarget(const Expr* expr, const SourcePosition*) override {}

  // CreateList node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateList(const CreateList* list_expr, const Expr* expr,
                           const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    AddStep(CreateCreateListStep(list_expr, expr->id()));
  }

  // CreateStruct node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateStruct(const CreateStruct* struct_expr, const Expr* expr,
                             const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    AddStep(CreateCreateStructStep(struct_expr, expr->id()));
  }

  absl::Status progress_status() const { return progress_status_; }

  void AddStep(absl::StatusOr<std::unique_ptr<ExpressionStep>> step_status) {
    if (step_status.ok() && progress_status_.ok()) {
      flattened_path_->push_back(std::move(step_status.value()));
    } else {
      SetProgressStatusError(step_status.status());
    }
  }

  void AddStep(std::unique_ptr<ExpressionStep> step) {
    if (progress_status_.ok()) {
      flattened_path_->push_back(std::move(step));
    }
  }

  void SetProgressStatusError(const absl::Status& status) {
    if (progress_status_.ok() && !status.ok()) {
      progress_status_ = status;
    }
  }

  // Index of the next step to be inserted.
  int GetCurrentIndex() const { return flattened_path_->size(); }

  CondVisitor* FindCondVisitor(const Expr* expr) const {
    if (cond_visitor_stack_.empty()) {
      return nullptr;
    }

    const auto& latest = cond_visitor_stack_.top();

    return (latest.first == expr) ? latest.second.get() : nullptr;
  }

 private:
  ExecutionPath* flattened_path_;
  absl::Status progress_status_;

  std::stack<std::pair<const Expr*, std::unique_ptr<CondVisitor>>>
      cond_visitor_stack_;

  // Maps effective namespace names to Expr objects (IDENTs/SELECTs) that
  // define scopes for those namespaces.
  std::unordered_map<const Expr*, std::string> namespace_map_;
  // Tracks SELECT-...SELECT-IDENT chains.
  std::deque<std::pair<const Expr*, std::string>> namespace_stack_;

  // When multiple SELECT-...SELECT-IDENT chain is resolved as namespace, this
  // field is used as marker suppressing CelExpression creation for SELECTs.
  const Expr* resolved_select_expr_;

  // Fully resolved enum value names.
  absl::node_hash_map<std::string, const google::protobuf::EnumValueDescriptor*>
      enum_map_;

  const CelFunctionRegistry* function_registry_;

  bool short_circuiting_;

  const absl::flat_hash_map<std::string, CelValue>& constant_idents_;

  bool enable_comprehension_;

  BuilderWarnings* builder_warnings_;

  std::set<std::string>* iter_variable_names_;
};

void BinaryCondVisitor::PreVisit(const Expr* expr) {
  if (expr->call_expr().args().size() != 2) {
    visitor_->SetProgressStatusError(absl::InvalidArgumentError(
        "Unexpected number of arguments in a binary function call."));
  }
}

void BinaryCondVisitor::PostVisitArg(int arg_num, const Expr* expr) {
  if (!short_circuiting_) {
    // nothing to do.
    return;
  }
  if (arg_num == 0) {
    // If first branch evaluation result is enough to determine output,
    // jump over the second branch and provide result as final output.
    auto jump_step_status =
        CreateCondJumpStep(cond_value_, true, {}, expr->id());
    if (jump_step_status.ok()) {
      jump_step_ =
          Jump(visitor_->GetCurrentIndex(), jump_step_status.value().get());
    }
    visitor_->AddStep(std::move(jump_step_status));
  }
}

void BinaryCondVisitor::PostVisit(const Expr* expr) {
  // TODO(issues/41): shortcircuit behavior is non-obvious: should add
  // documentation and structure the code a bit better.
  visitor_->AddStep((cond_value_) ? CreateOrStep(expr->id())
                                  : CreateAndStep(expr->id()));
  if (short_circuiting_) {
    jump_step_.set_target(visitor_->GetCurrentIndex());
  }
}

void TernaryCondVisitor::PreVisit(const Expr*) {}

void TernaryCondVisitor::PostVisitArg(int arg_num, const Expr* expr) {
  // Ternary operator "_?_:_" requires a special handing.
  // In contrary to regular function call, its execution affects the control
  // flow of the overall CEL expression.
  // If condition value (argument 0) is True, then control flow is unaffected
  // as it is passed to the first conditional branch. Then, at the end of this
  // branch, the jump is performed over the second conditional branch.
  // If condition value is False, then jump is performed and control is passed
  // to the beginning of the second conditional branch.
  // If condition value is Error, then jump is peformed to bypass both
  // conditional branches and provide Error as result of ternary operation.

  // condition argument for ternary operator
  if (arg_num == 0) {
    // Jump in case of error or non-bool
    auto error_jump_status = CreateBoolCheckJumpStep({}, expr->id());
    if (error_jump_status.ok()) {
      error_jump_ =
          Jump(visitor_->GetCurrentIndex(), error_jump_status.value().get());
    }
    visitor_->AddStep(std::move(error_jump_status));

    // Jump to the second branch of execution
    // Value is to be removed from the stack.
    auto jump_to_second_status =
        CreateCondJumpStep(false, false, {}, expr->id());
    if (jump_to_second_status.ok()) {
      jump_to_second_ = Jump(visitor_->GetCurrentIndex(),
                             jump_to_second_status.value().get());
    }
    visitor_->AddStep(std::move(jump_to_second_status));
  } else if (arg_num == 1) {
    // Jump after the first and over the second branch of execution.
    // Value is to be removed from the stack.
    auto jump_after_first_status = CreateJumpStep({}, expr->id());
    if (jump_after_first_status.ok()) {
      jump_after_first_ = Jump(visitor_->GetCurrentIndex(),
                               jump_after_first_status.value().get());
    }
    visitor_->AddStep(std::move(jump_after_first_status));

    if (jump_to_second_.exists()) {
      jump_to_second_.set_target(visitor_->GetCurrentIndex());
    } else {
      visitor_->SetProgressStatusError(absl::InvalidArgumentError(
          "Error configuring ternary operator: jump_to_second_ is null"));
    }
  }
  // Code executed after traversing the final branch of execution
  // (arg_num == 2) is placed in PostVisitCall, to make this method less
  // clattered.
}

void TernaryCondVisitor::PostVisit(const Expr*) {
  // Determine and set jump offset in jump instruction.
  if (error_jump_.exists()) {
    error_jump_.set_target(visitor_->GetCurrentIndex());
  } else {
    visitor_->SetProgressStatusError(absl::InvalidArgumentError(
        "Error configuring ternary operator: error_jump_ is null"));
    return;
  }
  if (jump_after_first_.exists()) {
    jump_after_first_.set_target(visitor_->GetCurrentIndex());
  } else {
    visitor_->SetProgressStatusError(absl::InvalidArgumentError(
        "Error configuring ternary operator: jump_after_first_ is null"));
    return;
  }
}

void ExhaustiveTernaryCondVisitor::PostVisit(const Expr* expr) {
  visitor_->AddStep(CreateTernaryStep(expr->id()));
}

const Expr* Int64ConstImpl(int64_t value) {
  Constant* constant = new Constant;
  constant->set_int64_value(value);
  Expr* expr = new Expr;
  expr->set_allocated_const_expr(constant);
  return expr;
}

const Expr* MinusOne() {
  static const Expr* expr = Int64ConstImpl(-1);
  return expr;
}

const Expr* LoopStepDummy() {
  static const Expr* expr = Int64ConstImpl(-10);
  return expr;
}

const Expr* CurrentValueDummy() {
  static const Expr* expr = Int64ConstImpl(-20);
  return expr;
}

void ComprehensionVisitor::PreVisit(const Expr*) {
  const Expr* dummy = LoopStepDummy();
  visitor_->AddStep(CreateConstValueStep(
      ConvertConstant(&dummy->const_expr()).value(), dummy->id(), false));
}

void ComprehensionVisitor::PostVisitArg(int arg_num, const Expr* expr) {
  const Comprehension* comprehension = &expr->comprehension_expr();
  const auto& accu_var = comprehension->accu_var();
  const auto& iter_var = comprehension->iter_var();
  // TODO(issues/20): Consider refactoring the comprehension prologue step.
  switch (arg_num) {
    case ITER_RANGE: {
      // Post-process iter_range to list its keys if it's a map.
      visitor_->AddStep(CreateListKeysStep(expr->id()));
      const Expr* minus1 = MinusOne();
      visitor_->AddStep(CreateConstValueStep(
          ConvertConstant(&minus1->const_expr()).value(), minus1->id(), false));
      const Expr* dummy = CurrentValueDummy();
      visitor_->AddStep(CreateConstValueStep(
          ConvertConstant(&dummy->const_expr()).value(), dummy->id(), false));
      break;
    }
    case ACCU_INIT: {
      next_step_pos_ = visitor_->GetCurrentIndex();
      next_step_ = new ComprehensionNextStep(accu_var, iter_var, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(next_step_));
      break;
    }
    case LOOP_CONDITION: {
      cond_step_pos_ = visitor_->GetCurrentIndex();
      cond_step_ = new ComprehensionCondStep(accu_var, iter_var,
                                             short_circuiting_, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(cond_step_));
      break;
    }
    case LOOP_STEP: {
      auto jump_to_next = CreateJumpStep(
          next_step_pos_ - visitor_->GetCurrentIndex() - 1, expr->id());
      if (jump_to_next.ok()) {
        visitor_->AddStep(std::move(jump_to_next));
      }
      // Set offsets.
      cond_step_->set_jump_offset(visitor_->GetCurrentIndex() - cond_step_pos_ -
                                  1);
      next_step_->set_jump_offset(visitor_->GetCurrentIndex() - next_step_pos_ -
                                  1);
      break;
    }
    case RESULT: {
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(
          new ComprehensionFinish(accu_var, iter_var, expr->id())));
      next_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        next_step_pos_ - 1);
      cond_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        cond_step_pos_ - 1);
      break;
    }
  }
}

void ComprehensionVisitor::PostVisit(const Expr*) {}

}  // namespace

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpressionImpl(
    const Expr* expr, const SourceInfo* source_info,
    const google::protobuf::Map<int64_t, Reference>* reference_map,
    std::vector<absl::Status>* warnings) const {
  ExecutionPath execution_path;
  BuilderWarnings warnings_builder(fail_on_warnings_);

  if (absl::StartsWith(container(), ".") || absl::EndsWith(container(), ".")) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid expression container:", container()));
  }

  absl::flat_hash_map<std::string, CelValue> idents;

  const Expr* effective_expr = expr;
  // transformed expression preserving expression IDs
  std::unique_ptr<Expr> rewrite_buffer = nullptr;
  // TODO(issues/98): A type checker may perform these rewrites, but there
  // currently  isn't a signal to expose that in an expression. If that becomes
  // available, we can skip the reference resolve step here if it's already
  // done.
  if (reference_map != nullptr && !reference_map->empty()) {
    absl::StatusOr<absl::optional<Expr>> rewritten =
        ResolveReferences(*effective_expr, *reference_map, *GetRegistry(),
                          container(), &warnings_builder);
    if (!rewritten.ok()) {
      return rewritten.status();
    }
    if (rewritten.value().has_value()) {
      rewrite_buffer =
          std::make_unique<Expr>(std::move(rewritten).value().value());
      effective_expr = rewrite_buffer.get();
    }
    // TODO(issues/99): we could setup a check step here that confirms all of
    // references are defined before actually evaluating.
  }

  Expr const_fold_buffer;
  if (constant_folding_) {
    FoldConstants(*effective_expr, *this->GetRegistry(), constant_arena_,
                  idents, &const_fold_buffer);
    effective_expr = &const_fold_buffer;
  }

  std::set<std::string> iter_variable_names;
  FlatExprVisitor visitor(this->GetRegistry(), &execution_path,
                          shortcircuiting_, resolvable_enums(), container(),
                          idents, enable_comprehension_, &warnings_builder,
                          &iter_variable_names);

  AstTraverse(effective_expr, source_info, &visitor);

  if (!visitor.progress_status().ok()) {
    return visitor.progress_status();
  }

  std::unique_ptr<CelExpression> expression_impl =
      absl::make_unique<CelExpressionFlatImpl>(
          expr, std::move(execution_path), comprehension_max_iterations_,
          std::move(iter_variable_names), enable_unknowns_,
          enable_unknown_function_results_, enable_missing_attribute_errors_,
          std::move(rewrite_buffer));

  if (warnings != nullptr) {
    *warnings = std::move(warnings_builder).warnings();
  }
  return std::move(expression_impl);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const Expr* expr,
                                  const SourceInfo* source_info,
                                  std::vector<absl::Status>* warnings) const {
  return CreateExpressionImpl(expr, source_info, /*reference_map=*/nullptr,
                              warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const Expr* expr,
                                  const SourceInfo* source_info) const {
  return CreateExpressionImpl(expr, source_info, /*reference_map=*/nullptr,
                              /*warnings=*/nullptr);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const CheckedExpr* checked_expr,
                                  std::vector<absl::Status>* warnings) const {
  return CreateExpressionImpl(&checked_expr->expr(),
                              &checked_expr->source_info(),
                              &checked_expr->reference_map(), warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const CheckedExpr* checked_expr) const {
  return CreateExpressionImpl(&checked_expr->expr(),
                              &checked_expr->source_info(),
                              &checked_expr->reference_map(),
                              /*warnings=*/nullptr);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
