#include "eval/compiler/constant_folding.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/ast_internal.h"
#include "base/builtins.h"
#include "base/function.h"
#include "base/handle.h"
#include "base/internal/ast_impl.h"
#include "base/kind.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/values/bytes_value.h"
#include "base/values/error_value.h"
#include "base/values/string_value.h"
#include "base/values/unknown_value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/internal/errors.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "runtime/activation.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"

namespace cel::ast::internal {

namespace {

using ::cel::builtin::kAnd;
using ::cel::builtin::kOr;
using ::cel::builtin::kTernary;
using ::cel::extensions::ProtoMemoryManager;
using ::cel::interop_internal::CreateErrorValueFromView;
using ::cel::interop_internal::CreateLegacyListValue;
using ::cel::interop_internal::CreateNoMatchingOverloadError;
using ::cel::interop_internal::ModernValueToLegacyValueOrDie;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ContainerBackedListImpl;
using ::google::api::expr::runtime::EvaluationListener;
using ::google::api::expr::runtime::ExecutionFrame;
using ::google::api::expr::runtime::ExecutionPath;
using ::google::api::expr::runtime::ExecutionPathView;
using ::google::api::expr::runtime::FlatExpressionEvaluatorState;
using ::google::api::expr::runtime::PlannerContext;
using ::google::api::expr::runtime::ProgramOptimizer;
using ::google::api::expr::runtime::Resolver;

using ::google::protobuf::Arena;

Handle<Value> CreateLegacyListBackedHandle(
    Arena* arena, const std::vector<Handle<Value>>& values) {
  std::vector<CelValue> legacy_values =
      ModernValueToLegacyValueOrDie(arena, values);

  const auto* legacy_list =
      Arena::Create<google::api::expr::runtime::ContainerBackedListImpl>(
          arena, std::move(legacy_values));

  return CreateLegacyListValue(legacy_list);
}

absl::optional<std::string> ToQualifiedIdentifier(const Select& select) {
  if (select.test_only()) {
    return absl::nullopt;
  }
  if (select.operand().has_select_expr()) {
    auto op_name = ToQualifiedIdentifier(select.operand().select_expr());
    if (op_name.has_value()) {
      return absl::StrCat(*std::move(op_name), ".", select.field());
    }
    return absl::nullopt;
  }
  if (select.operand().has_ident_expr()) {
    return absl::StrCat(select.operand().ident_expr().name(), ".",
                        select.field());
  }
  return absl::nullopt;
}

struct MakeConstantArenaSafeVisitor {
  // TODO(uncreated-issue/33): make the AST to runtime Value conversion work with
  // non-arena based cel::MemoryManager.
  google::protobuf::Arena* arena;

  Handle<Value> operator()(const cel::ast::internal::NullValue& value) {
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
  Handle<Value> operator()(const cel::ast::internal::Bytes& value) {
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
    google::protobuf::Arena* arena, const cel::ast::internal::Constant& const_expr) {
  return absl::visit(MakeConstantArenaSafeVisitor{arena},
                     const_expr.constant_kind());
}

class ConstantFoldingTransform {
 public:
  ConstantFoldingTransform(
      const FunctionRegistry& registry, google::protobuf::Arena* arena,
      absl::flat_hash_map<std::string, Handle<Value>>& constant_idents)
      : registry_(registry),
        arena_(arena),
        memory_manager_(arena),
        type_factory_(memory_manager_),
        type_manager_(type_factory_, TypeProvider::Builtin()),
        value_factory_(type_manager_),
        constant_idents_(constant_idents),
        counter_(0) {}

  // Copies the expression, replacing constant sub-expressions with identifiers
  // mapping to Handle<Value> values. Returns true if this expression (including
  // all subexpressions) is a constant.
  bool Transform(const Expr& expr, Expr& out);

  void MakeConstant(Handle<Value> value, Expr& out) {
    auto ident = absl::StrCat("$v", counter_++);
    constant_idents_.insert_or_assign(ident, std::move(value));
    out.mutable_ident_expr().set_name(ident);
  }

  Handle<Value> RemoveConstant(const Expr& ident) {
    // absl utility function: find, remove and return the underlying map node.
    return std::move(
        constant_idents_.extract(ident.ident_expr().name()).mapped());
  }

 private:
  class ConstFoldingVisitor {
   public:
    ConstFoldingVisitor(const Expr& input, ConstantFoldingTransform& transform,
                        Expr& output)
        : expr_(input), transform_(transform), out_(output) {}
    bool operator()(const Constant& constant) {
      // create a constant that references the input expression data
      // since the output expression is temporary
      auto value = MakeConstantArenaSafe(transform_.arena_, constant);
      if (value) {
        transform_.MakeConstant(std::move(value), out_);
        return true;
      } else {
        out_.mutable_const_expr() = expr_.const_expr();
        return false;
      }
    }

    bool operator()(const Ident& ident) {
      // TODO(uncreated-issue/34): this could be updated to use the rewrite visitor
      // to make changes in-place instead of manually copy. This would avoid
      // having to understand how to copy all of the information in the original
      // AST.
      out_.mutable_ident_expr().set_name(expr_.ident_expr().name());
      return false;
    }

    bool operator()(const Select& select) {
      auto& select_expr = out_.mutable_select_expr();
      transform_.Transform(expr_.select_expr().operand(),
                           select_expr.mutable_operand());
      select_expr.set_field(expr_.select_expr().field());
      select_expr.set_test_only(expr_.select_expr().test_only());
      return false;
    }

    bool operator()(const Call& call) {
      auto& call_expr = out_.mutable_call_expr();
      const bool receiver_style = expr_.call_expr().has_target();
      const int arg_num = expr_.call_expr().args().size();
      bool all_constant = true;
      if (receiver_style) {
        all_constant = transform_.Transform(expr_.call_expr().target(),
                                            call_expr.mutable_target()) &&
                       all_constant;
      }
      call_expr.set_function(expr_.call_expr().function());
      for (int i = 0; i < arg_num; i++) {
        all_constant =
            transform_.Transform(expr_.call_expr().args()[i],
                                 call_expr.mutable_args().emplace_back()) &&
            all_constant;
      }
      // short-circuiting affects evaluation of logic combinators, so we do
      // not fold them here
      if (!all_constant || call_expr.function() == cel::builtin::kAnd ||
          call_expr.function() == kOr || call_expr.function() == kTernary) {
        return false;
      }

      // compute argument list
      const int arg_size = arg_num + (receiver_style ? 1 : 0);
      std::vector<Kind> arg_types(arg_size, Kind::kAny);
      auto overloads = transform_.registry_.FindStaticOverloads(
          call_expr.function(), receiver_style, arg_types);

      // do not proceed if there are no overloads registered
      if (overloads.empty()) {
        return false;
      }

      std::vector<Handle<Value>> arg_values;
      std::vector<cel::Kind> arg_kinds;
      arg_values.reserve(arg_size);
      arg_kinds.reserve(arg_size);
      if (receiver_style) {
        arg_values.push_back(transform_.RemoveConstant(call_expr.target()));
        arg_kinds.push_back(ValueKindToKind(arg_values.back()->kind()));
      }
      for (int i = 0; i < arg_num; i++) {
        arg_values.push_back(transform_.RemoveConstant(call_expr.args()[i]));
        arg_kinds.push_back(ValueKindToKind(arg_values.back()->kind()));
      }

      // compute function overload
      // consider consolidating this logic with FunctionStep overload
      // resolution.
      absl::optional<FunctionOverloadReference> matched_function;
      for (auto overload : overloads) {
        if (overload.descriptor.ShapeMatches(receiver_style, arg_kinds)) {
          matched_function.emplace(overload);
        }
      }
      if (!matched_function.has_value() ||
          matched_function->descriptor.is_strict()) {
        // propagate argument errors up the expression
        for (Handle<Value>& arg : arg_values) {
          if (arg->Is<ErrorValue>()) {
            transform_.MakeConstant(std::move(arg), out_);
            return true;
          }
        }
      }
      if (!matched_function.has_value()) {
        Handle<Value> error =
            CreateErrorValueFromView(CreateNoMatchingOverloadError(
                transform_.arena_, call_expr.function()));
        transform_.MakeConstant(std::move(error), out_);
        return true;
      }

      FunctionEvaluationContext context(transform_.value_factory_);
      auto call_result =
          matched_function->implementation.Invoke(context, arg_values);

      if (call_result.ok()) {
        transform_.MakeConstant(std::move(call_result).value(), out_);
      } else {
        Handle<Value> error =
            CreateErrorValueFromView(Arena::Create<absl::Status>(
                transform_.arena_, std::move(call_result).status()));
        transform_.MakeConstant(std::move(error), out_);
      }
      return true;
    }

    bool operator()(const CreateList& list) {
      auto& list_expr = out_.mutable_list_expr();
      int list_size = expr_.list_expr().elements().size();
      bool all_constant = true;
      for (int i = 0; i < list_size; i++) {
        auto& element = list_expr.mutable_elements().emplace_back();
        // TODO(uncreated-issue/34): Add support for CEL optional.
        all_constant =
            transform_.Transform(expr_.list_expr().elements()[i], element) &&
            all_constant;
      }

      if (!all_constant) {
        return false;
      }

      if (list_size == 0) {
        // TODO(uncreated-issue/35): need a more robust fix to support generic
        // comprehensions, but this will allow comprehension list append
        // optimization to work to prevent quadratic memory consumption for
        // map/filter.
        return false;
      }

      // create a constant list value
      std::vector<Handle<Value>> values(list_size);
      for (int i = 0; i < list_size; i++) {
        values[i] = transform_.RemoveConstant(list_expr.elements()[i]);
      }

      Handle<Value> cel_list =
          CreateLegacyListBackedHandle(transform_.arena_, values);
      transform_.MakeConstant(std::move(cel_list), out_);
      return true;
    }

    bool operator()(const CreateStruct& create_struct) {
      auto& struct_expr = out_.mutable_struct_expr();
      struct_expr.set_message_name(expr_.struct_expr().message_name());
      int entries_size = expr_.struct_expr().entries().size();
      for (int i = 0; i < entries_size; i++) {
        auto& entry = expr_.struct_expr().entries()[i];
        auto& new_entry = struct_expr.mutable_entries().emplace_back();
        new_entry.set_id(entry.id());
        struct {
          // TODO(uncreated-issue/34): Add support for CEL optional.
          ConstantFoldingTransform& transform;
          const CreateStruct::Entry& entry;
          CreateStruct::Entry& new_entry;

          void operator()(const std::string& key) {
            new_entry.set_field_key(key);
          }

          void operator()(const std::unique_ptr<Expr>& expr) {
            transform.Transform(entry.map_key(), new_entry.mutable_map_key());
          }
        } handler{transform_, entry, new_entry};
        absl::visit(handler, entry.key_kind());
        transform_.Transform(entry.value(), new_entry.mutable_value());
      }
      return false;
    }

    bool operator()(const Comprehension& comprehension) {
      // do not fold comprehensions for now: would require significant
      // factoring out of comprehension semantics from the evaluator
      auto& input_expr = expr_.comprehension_expr();
      auto& out_expr = out_.mutable_comprehension_expr();
      out_expr.set_iter_var(input_expr.iter_var());
      transform_.Transform(input_expr.accu_init(),
                           out_expr.mutable_accu_init());
      transform_.Transform(input_expr.iter_range(),
                           out_expr.mutable_iter_range());
      out_expr.set_accu_var(input_expr.accu_var());
      transform_.Transform(input_expr.loop_condition(),
                           out_expr.mutable_loop_condition());
      transform_.Transform(input_expr.loop_step(),
                           out_expr.mutable_loop_step());
      transform_.Transform(input_expr.result(), out_expr.mutable_result());
      return false;
    }

    bool operator()(absl::monostate) {
      ABSL_LOG(ERROR) << "Unsupported Expr kind";
      return false;
    }

   private:
    const Expr& expr_;
    ConstantFoldingTransform& transform_;
    Expr& out_;
  };
  const FunctionRegistry& registry_;

  // Owns constant values created during folding
  Arena* arena_;
  // TODO(uncreated-issue/33): make this support generic memory manager and value
  // factory. This is only safe for interop where we know an arena is always
  // available.
  extensions::ProtoMemoryManager memory_manager_;
  TypeFactory type_factory_;
  TypeManager type_manager_;
  ValueFactory value_factory_;
  absl::flat_hash_map<std::string, Handle<Value>>& constant_idents_;

  int counter_;
};

bool ConstantFoldingTransform::Transform(const Expr& expr, Expr& out_) {
  out_.set_id(expr.id());
  ConstFoldingVisitor handler(expr, *this, out_);
  return absl::visit(handler, expr.expr_kind());
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
    kConst,
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
    IsConst operator()(const Constant&) { return IsConst::kConst; }
    IsConst operator()(const Ident& ident) {
      if (resolver.FindConstant(ident.name(), expr_id)) {
        return IsConst::kConst;
      }
      return IsConst::kNonConst;
    }
    IsConst operator()(const Comprehension&) {
      // Not yet supported, need to identify whether range and
      // iter vars are compatible with const folding.
      return IsConst::kNonConst;
    }
    IsConst operator()(const CreateStruct&) {
      // Not yet supported but should be possible in the future.
      return IsConst::kNonConst;
    }
    IsConst operator()(const CreateList& create_list) {
      if (create_list.elements().empty()) {
        // TODO(uncreated-issue/35): Don't fold for empty list to allow comprehension
        // list append optimization.
        return IsConst::kNonConst;
      }
      return IsConst::kConditional;
    }

    IsConst operator()(const Select& select) {
      auto qual_id = ToQualifiedIdentifier(select);
      if (qual_id.has_value() && resolver.FindConstant(*qual_id, expr_id)) {
        return IsConst::kConst;
      }
      return IsConst::kConditional;
    }

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
    const int64_t expr_id;
  };

  IsConst is_const = absl::visit(IsConstVisitor{context.resolver(), node.id()},
                                 node.expr_kind());
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
    if (!is_const_.empty() && is_const_.back() == IsConst::kConditional) {
      is_const_.back() = IsConst::kNonConst;
    }
    return absl::OkStatus();
  }

  // copy string to arena if backed by the original program.
  Handle<Value> value;
  if (node.has_const_expr()) {
    value = absl::visit(MakeConstantArenaSafeVisitor{arena_},
                        node.const_expr().constant_kind());
  } else if (node.has_ident_expr()) {
    value =
        context.resolver().FindConstant(node.ident_expr().name(), node.id());
  } else if (node.has_select_expr()) {
    auto qual_id = ToQualifiedIdentifier(node.select_expr());
    if (qual_id.has_value()) {
      value = context.resolver().FindConstant(qual_id.value(), node.id());
    }
  }
  if (!value) {
    ExecutionPathView subplan = context.GetSubplan(node);
    ExecutionFrame frame(subplan, empty_, context.options(), state_);
    state_.Reset();
    // Update stack size to accommodate sub expression.
    // This only results in a vector resize if the new maxsize is greater than
    // the current capacity.
    state_.value_stack().SetMaxSize(subplan.size());

    CEL_ASSIGN_OR_RETURN(value, frame.Evaluate(null_listener_));
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

void FoldConstants(
    const Expr& ast, const FunctionRegistry& registry, google::protobuf::Arena* arena,
    absl::flat_hash_map<std::string, Handle<Value>>& constant_idents,
    Expr& out_ast) {
  ConstantFoldingTransform constant_folder(registry, arena, constant_idents);
  constant_folder.Transform(ast, out_ast);
}

google::api::expr::runtime::ProgramOptimizerFactory
CreateConstantFoldingExtension(google::protobuf::Arena* arena) {
  return [=](PlannerContext& ctx, const AstImpl&) {
    return std::make_unique<ConstantFoldingExtension>(
        arena, ctx.type_registry().GetTypeProvider());
  };
}

}  // namespace cel::ast::internal
