#include "eval/compiler/constant_folding.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/builtins.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/values/unknown_value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/interop.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "runtime/activation.h"
#include "google/protobuf/arena.h"

namespace cel::ast_internal {

namespace {

using ::cel::builtin::kAnd;
using ::cel::builtin::kOr;
using ::cel::builtin::kTernary;
using ::cel::extensions::ProtoMemoryManager;
using ::google::api::expr::runtime::EvaluationListener;
using ::google::api::expr::runtime::ExecutionFrame;
using ::google::api::expr::runtime::ExecutionPath;
using ::google::api::expr::runtime::ExecutionPathView;
using ::google::api::expr::runtime::FlatExpressionEvaluatorState;
using ::google::api::expr::runtime::PlannerContext;
using ::google::api::expr::runtime::ProgramOptimizer;
using ::google::api::expr::runtime::Resolver;

using ::google::protobuf::Arena;

struct MakeConstantArenaSafeVisitor {
  // TODO(uncreated-issue/33): make the AST to runtime Value conversion work with
  // non-arena based cel::MemoryManager.
  google::protobuf::Arena* arena;

  Handle<Value> operator()(const cel::ast_internal::NullValue& value) {
    return cel::interop_internal::CreateNullValue();
  }
  Handle<Value> operator()(bool value) {
    return cel::interop_internal::CreateBoolValue(value);
  }
  Handle<Value> operator()(int64_t value) {
    return cel::interop_internal::CreateIntValue(value);
  }
  Handle<Value> operator()(uint64_t value) {
    return cel::interop_internal::CreateUintValue(value);
  }
  Handle<Value> operator()(double value) {
    return cel::interop_internal::CreateDoubleValue(value);
  }
  Handle<Value> operator()(const std::string& value) {
    const auto* arena_copy = Arena::Create<std::string>(arena, value);
    return cel::interop_internal::CreateStringValueFromView(*arena_copy);
  }
  Handle<Value> operator()(const cel::ast_internal::Bytes& value) {
    const auto* arena_copy = Arena::Create<std::string>(arena, value.bytes);
    return cel::interop_internal::CreateBytesValueFromView(*arena_copy);
  }
  Handle<Value> operator()(const absl::Duration duration) {
    return cel::interop_internal::CreateDurationValue(duration);
  }
  Handle<Value> operator()(const absl::Time timestamp) {
    return cel::interop_internal::CreateTimestampValue(timestamp);
  }
};

Handle<Value> MakeConstantArenaSafe(
    google::protobuf::Arena* arena, const cel::ast_internal::Constant& const_expr) {
  return absl::visit(MakeConstantArenaSafeVisitor{arena},
                     const_expr.constant_kind());
}

class ConstantFoldingExtension : public ProgramOptimizer {
 public:
  explicit ConstantFoldingExtension(google::protobuf::Arena* arena,
                                    const TypeProvider& type_provider)
      : arena_(arena),
        memory_manager_(arena),
        state_(kDefaultStackLimit, type_provider, memory_manager_) {}

  absl::Status OnPreVisit(google::api::expr::runtime::PlannerContext& context,
                          const Expr& node) override;
  absl::Status OnPostVisit(google::api::expr::runtime::PlannerContext& context,
                           const Expr& node) override;

 private:
  enum class IsConst {
    kConditional,
    kNonConst,
  };
  // Most constant folding evaluations are simple
  // binary operators.
  static constexpr size_t kDefaultStackLimit = 4;

  google::protobuf::Arena* arena_;
  ProtoMemoryManager memory_manager_;
  Activation empty_;
  EvaluationListener null_listener_;
  FlatExpressionEvaluatorState state_;

  std::vector<IsConst> is_const_;
};

absl::Status ConstantFoldingExtension::OnPreVisit(PlannerContext& context,
                                                  const Expr& node) {
  struct IsConstVisitor {
    IsConst operator()(const Constant&) { return IsConst::kConditional; }
    IsConst operator()(const Ident&) { return IsConst::kNonConst; }
    IsConst operator()(const Comprehension&) {
      // Not yet supported, need to identify whether range and
      // iter vars are compatible with const folding.
      return IsConst::kNonConst;
    }
    IsConst operator()(const CreateStruct& create_struct) {
      // Not yet supported but should be possible in the future.
      // Empty maps are rare and not currently supported as they may eventually
      // have similar issues to empty list when used within comprehensions or
      // macros.
      if (create_struct.entries().empty() ||
          !create_struct.message_name().empty()) {
        return IsConst::kNonConst;
      }
      return IsConst::kConditional;
    }
    IsConst operator()(const CreateList& create_list) {
      if (create_list.elements().empty()) {
        // TODO(uncreated-issue/35): Don't fold for empty list to allow comprehension
        // list append optimization.
        return IsConst::kNonConst;
      }
      return IsConst::kConditional;
    }

    IsConst operator()(const Select&) { return IsConst::kConditional; }

    IsConst operator()(absl::monostate) { return IsConst::kNonConst; }

    IsConst operator()(const Call& call) {
      // Short Circuiting operators not yet supported.
      if (call.function() == kAnd || call.function() == kOr ||
          call.function() == kTernary) {
        return IsConst::kNonConst;
      }

      int arg_len = call.args().size() + (call.has_target() ? 1 : 0);
      std::vector<cel::Kind> arg_matcher(arg_len, cel::Kind::kAny);
      // Check for any lazy overloads (activation dependant)
      if (!resolver
               .FindLazyOverloads(call.function(), call.has_target(),
                                  arg_matcher)
               .empty()) {
        return IsConst::kNonConst;
      }

      return IsConst::kConditional;
    }

    const Resolver& resolver;
  };

  IsConst is_const =
      absl::visit(IsConstVisitor{context.resolver()}, node.expr_kind());
  is_const_.push_back(is_const);

  return absl::OkStatus();
}

absl::Status ConstantFoldingExtension::OnPostVisit(PlannerContext& context,
                                                   const Expr& node) {
  if (is_const_.empty()) {
    return absl::InternalError("ConstantFoldingExtension called out of order.");
  }

  IsConst is_const = is_const_.back();
  is_const_.pop_back();

  if (is_const == IsConst::kNonConst) {
    // update parent
    if (!is_const_.empty()) {
      is_const_.back() = IsConst::kNonConst;
    }
    return absl::OkStatus();
  }

  // copy string to arena if backed by the original program.
  Handle<Value> value;
  if (node.has_const_expr()) {
    value = absl::visit(MakeConstantArenaSafeVisitor{arena_},
                        node.const_expr().constant_kind());
  } else {
    ExecutionPathView subplan = context.GetSubplan(node);
    ExecutionFrame frame(subplan, empty_, context.options(), state_);
    state_.Reset();
    // Update stack size to accommodate sub expression.
    // This only results in a vector resize if the new maxsize is greater than
    // the current capacity.
    state_.value_stack().SetMaxSize(subplan.size());

    auto result = frame.Evaluate(null_listener_);
    // If this would be a runtime error, then don't adjust the program plan, but
    // rather allow the error to occur at runtime to preserve the evaluation
    // contract with non-constant folding use cases.
    if (!result.ok()) {
      return absl::OkStatus();
    }
    value = *result;
    if (value->Is<UnknownValue>()) {
      return absl::OkStatus();
    }
  }

  ExecutionPath new_plan;
  CEL_ASSIGN_OR_RETURN(new_plan.emplace_back(),
                       google::api::expr::runtime::CreateConstValueStep(
                           std::move(value), node.id(), false));

  return context.ReplaceSubplan(node, std::move(new_plan));
}

}  // namespace

google::api::expr::runtime::ProgramOptimizerFactory
CreateConstantFoldingExtension(google::protobuf::Arena* arena) {
  return [=](PlannerContext& ctx, const AstImpl&) {
    return std::make_unique<ConstantFoldingExtension>(
        arena, ctx.value_factory().type_provider());
  };
}

}  // namespace cel::ast_internal
