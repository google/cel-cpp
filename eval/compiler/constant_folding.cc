#include "eval/compiler/constant_folding.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/ast_internal.h"
#include "base/values/error_value.h"
#include "eval/internal/errors.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "internal/status_macros.h"

namespace cel::ast::internal {

namespace {

using ::cel::interop_internal::CreateErrorValueFromView;
using ::cel::interop_internal::CreateLegacyListValue;
using ::cel::interop_internal::CreateNoMatchingOverloadError;
using ::cel::interop_internal::LegacyValueToModernValueOrDie;
using ::cel::interop_internal::ModernValueToLegacyValueOrDie;
using ::google::api::expr::runtime::CelFunction;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ContainerBackedListImpl;
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

absl::StatusOr<Handle<Value>> EvalLegacyFunction(
    Arena* arena, const std::vector<Handle<Value>>& arg_values,
    const CelFunction& matched_function) {
  CelValue result;
  std::vector<CelValue> legacy_args =
      ModernValueToLegacyValueOrDie(arena, arg_values);

  CEL_RETURN_IF_ERROR(matched_function.Evaluate(legacy_args, &result, arena));

  return LegacyValueToModernValueOrDie(arena, result);
}

struct MakeConstantArenaSafeVisitor {
  // TODO(issues/5): make the AST to runtime Value conversion work with
  // non-arena based cel::MemoryManager.
  google::protobuf::Arena* arena;

  cel::Handle<cel::Value> operator()(
      const cel::ast::internal::NullValue& value) {
    return cel::interop_internal::CreateNullValue();
  }
  cel::Handle<cel::Value> operator()(bool value) {
    return cel::interop_internal::CreateBoolValue(value);
  }
  cel::Handle<cel::Value> operator()(int64_t value) {
    return cel::interop_internal::CreateIntValue(value);
  }
  cel::Handle<cel::Value> operator()(uint64_t value) {
    return cel::interop_internal::CreateUintValue(value);
  }
  cel::Handle<cel::Value> operator()(double value) {
    return cel::interop_internal::CreateDoubleValue(value);
  }
  cel::Handle<cel::Value> operator()(const std::string& value) {
    const auto* arena_copy = Arena::Create<std::string>(arena, value);
    return cel::interop_internal::CreateStringValueFromView(*arena_copy);
  }
  cel::Handle<cel::Value> operator()(const cel::ast::internal::Bytes& value) {
    const auto* arena_copy = Arena::Create<std::string>(arena, value.bytes);
    return cel::interop_internal::CreateBytesValueFromView(*arena_copy);
  }
  cel::Handle<cel::Value> operator()(const absl::Duration duration) {
    return cel::interop_internal::CreateDurationValue(duration);
  }
  cel::Handle<cel::Value> operator()(const absl::Time timestamp) {
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
      const google::api::expr::runtime::CelFunctionRegistry& registry,
      google::protobuf::Arena* arena,
      absl::flat_hash_map<std::string, Handle<Value>>& constant_idents)
      : registry_(registry),
        arena_(arena),
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
      if (!all_constant ||
          call_expr.function() == google::api::expr::runtime::builtin::kAnd ||
          call_expr.function() == google::api::expr::runtime::builtin::kOr ||
          call_expr.function() ==
              google::api::expr::runtime::builtin::kTernary) {
        return false;
      }

      // compute argument list
      const int arg_size = arg_num + (receiver_style ? 1 : 0);
      std::vector<Kind> arg_types(arg_size, Kind::kAny);
      auto overloads = transform_.registry_.FindOverloads(
          call_expr.function(), receiver_style, arg_types);

      // do not proceed if there are no overloads registered
      if (overloads.empty()) {
        return false;
      }

      std::vector<Handle<Value>> arg_values;
      arg_values.reserve(arg_size);
      if (receiver_style) {
        arg_values.push_back(transform_.RemoveConstant(call_expr.target()));
      }
      for (int i = 0; i < arg_num; i++) {
        arg_values.push_back(transform_.RemoveConstant(call_expr.args()[i]));
      }

      // compute function overload
      // consider consolidating this logic with FunctionStep overload
      // resolution.
      const CelFunction* matched_function = nullptr;
      for (auto overload : overloads) {
        if (overload->MatchArguments(arg_values)) {
          matched_function = overload;
        }
      }
      if (matched_function == nullptr ||
          matched_function->descriptor().is_strict()) {
        // propagate argument errors up the expression
        for (Handle<Value>& arg : arg_values) {
          if (arg->Is<ErrorValue>()) {
            transform_.MakeConstant(std::move(arg), out_);
            return true;
          }
        }
      }
      if (matched_function == nullptr) {
        Handle<Value> error =
            CreateErrorValueFromView(CreateNoMatchingOverloadError(
                transform_.arena_, call_expr.function()));
        transform_.MakeConstant(std::move(error), out_);
        return true;
      }

      auto call_result =
          EvalLegacyFunction(transform_.arena_, arg_values, *matched_function);

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
        all_constant =
            transform_.Transform(expr_.list_expr().elements()[i], element) &&
            all_constant;
      }
      if (!all_constant) {
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
      // do not fold comprehensions for now: would require significal
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
      LOG(ERROR) << "Unsupported Expr kind";
      return false;
    }

   private:
    const Expr& expr_;
    ConstantFoldingTransform& transform_;
    Expr& out_;
  };
  const google::api::expr::runtime::CelFunctionRegistry& registry_;

  // Owns constant values created during folding
  Arena* arena_;
  absl::flat_hash_map<std::string, Handle<Value>>& constant_idents_;

  int counter_;
};

bool ConstantFoldingTransform::Transform(const Expr& expr, Expr& out_) {
  out_.set_id(expr.id());
  ConstFoldingVisitor handler(expr, *this, out_);
  return absl::visit(handler, expr.expr_kind());
}

}  // namespace

void FoldConstants(
    const Expr& ast,
    const google::api::expr::runtime::CelFunctionRegistry& registry,
    google::protobuf::Arena* arena,
    absl::flat_hash_map<std::string, Handle<Value>>& constant_idents,
    Expr& out_ast) {
  ConstantFoldingTransform constant_folder(registry, arena, constant_idents);
  constant_folder.Transform(ast, out_ast);
}

}  // namespace cel::ast::internal
