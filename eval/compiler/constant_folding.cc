#include "eval/compiler/constant_folding.h"

#include "absl/strings/str_cat.h"
#include "eval/eval/const_value_step.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/containers/container_backed_list_impl.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::Expr;

class ConstantFoldingTransform {
 public:
  ConstantFoldingTransform(
      const CelFunctionRegistry& registry, google::protobuf::Arena* arena,
      absl::flat_hash_map<std::string, CelValue>& constant_idents)
      : registry_(registry),
        arena_(arena),
        constant_idents_(constant_idents),
        counter_(0) {}

  // Copies the expression by pulling out constant sub-expressions into
  // CelValue idents. Returns true if the expression is a constant.
  bool Transform(const Expr& expr, Expr* out) {
    out->set_id(expr.id());
    switch (expr.expr_kind_case()) {
      case Expr::kConstExpr: {
        // create a constant that references the input expression data
        // since the output expression is temporary
        auto value = ConvertConstant(&expr.const_expr());
        if (value.has_value()) {
          makeConstant(*value, out);
          return true;
        } else {
          out->mutable_const_expr()->MergeFrom(expr.const_expr());
          return false;
        }
      }
      case Expr::kIdentExpr:
        out->mutable_ident_expr()->set_name(expr.ident_expr().name());
        return false;
      case Expr::kSelectExpr: {
        auto select_expr = out->mutable_select_expr();
        Transform(expr.select_expr().operand(), select_expr->mutable_operand());
        select_expr->set_field(expr.select_expr().field());
        select_expr->set_test_only(expr.select_expr().test_only());
        return false;
      }
      case Expr::kCallExpr: {
        auto call_expr = out->mutable_call_expr();
        const bool receiver_style = expr.call_expr().has_target();
        const int arg_num = expr.call_expr().args_size();
        bool all_constant = true;
        if (receiver_style) {
          all_constant = Transform(expr.call_expr().target(),
                                   call_expr->mutable_target()) &&
                         all_constant;
        }
        call_expr->set_function(expr.call_expr().function());
        for (int i = 0; i < arg_num; i++) {
          all_constant =
              Transform(expr.call_expr().args(i), call_expr->add_args()) &&
              all_constant;
        }
        // short-circuiting affects evaluation of logic combinators, so we do
        // not fold them here
        if (!all_constant || call_expr->function() == builtin::kAnd ||
            call_expr->function() == builtin::kOr ||
            call_expr->function() == builtin::kTernary) {
          return false;
        }

        // compute argument list
        const int arg_size = arg_num + (receiver_style ? 1 : 0);
        std::vector<CelValue::Type> arg_types(arg_size, CelValue::Type::kAny);
        auto overloads = registry_.FindOverloads(call_expr->function(),
                                                 receiver_style, arg_types);

        // do not proceed if there are no overloads registered
        if (overloads.empty()) {
          return false;
        }

        std::vector<CelValue> arg_values;
        arg_values.reserve(arg_size);
        if (receiver_style) {
          arg_values.push_back(removeConstant(call_expr->target()));
        }
        for (int i = 0; i < arg_num; i++) {
          arg_values.push_back(removeConstant(call_expr->args(i)));
        }

        // compute function overload
        // consider consolidating the logic with FunctionStep
        const CelFunction* matched_function = nullptr;
        for (auto overload : overloads) {
          if (overload->MatchArguments(arg_values)) {
            matched_function = overload;
          }
        }
        if (matched_function == nullptr) {
          // propagate argument errors up the expression
          for (const CelValue& arg : arg_values) {
            if (arg.IsError()) {
              makeConstant(arg, out);
              return true;
            }
          }
          makeConstant(
              CreateNoMatchingOverloadError(arena_, call_expr->function()),
              out);
          return true;
        }
        CelValue result;
        auto status = matched_function->Evaluate(arg_values, &result, arena_);
        if (status.ok()) {
          makeConstant(result, out);
        } else {
          makeConstant(
              CreateErrorValue(arena_, status.message(), status.code()), out);
        }
        return true;
      }
      case Expr::kListExpr: {
        auto list_expr = out->mutable_list_expr();
        int list_size = expr.list_expr().elements_size();
        bool all_constant = true;
        for (int i = 0; i < list_size; i++) {
          auto elt = list_expr->add_elements();
          all_constant =
              Transform(expr.list_expr().elements(i), elt) && all_constant;
        }
        if (!all_constant) {
          return false;
        }

        // create a constant list value
        std::vector<CelValue> values(list_size);
        for (int i = 0; i < list_size; i++) {
          values[i] = removeConstant(list_expr->elements(i));
        }
        CelList* cel_list = google::protobuf::Arena::Create<ContainerBackedListImpl>(
            arena_, std::move(values));
        makeConstant(CelValue::CreateList(cel_list), out);
        return true;
      }
      case Expr::kStructExpr: {
        auto struct_expr = out->mutable_struct_expr();
        struct_expr->set_message_name(expr.struct_expr().message_name());
        int entries_size = expr.struct_expr().entries_size();
        for (int i = 0; i < entries_size; i++) {
          auto& entry = expr.struct_expr().entries(i);
          auto new_entry = struct_expr->add_entries();
          new_entry->set_id(entry.id());
          switch (entry.key_kind_case()) {
            case Expr::CreateStruct::Entry::kFieldKey:
              new_entry->set_field_key(entry.field_key());
              break;
            case Expr::CreateStruct::Entry::kMapKey:
              Transform(entry.map_key(), new_entry->mutable_map_key());
              break;
            default:
              GOOGLE_LOG(ERROR) << "Unsupported Entry kind: " << entry.key_kind_case();
              break;
          }
          Transform(entry.value(), new_entry->mutable_value());
        }
        return false;
      }
      case Expr::kComprehensionExpr: {
        // do not fold comprehensions for now: would require significal
        // factoring out of comprehension semantics from the evaluator
        auto& input_expr = expr.comprehension_expr();
        auto out_expr = out->mutable_comprehension_expr();
        out_expr->set_iter_var(input_expr.iter_var());
        Transform(input_expr.accu_init(), out_expr->mutable_accu_init());
        Transform(input_expr.iter_range(), out_expr->mutable_iter_range());
        out_expr->set_accu_var(input_expr.accu_var());
        Transform(input_expr.loop_condition(),
                  out_expr->mutable_loop_condition());
        Transform(input_expr.loop_step(), out_expr->mutable_loop_step());
        Transform(input_expr.result(), out_expr->mutable_result());
        return false;
      }
      default:
        GOOGLE_LOG(ERROR) << "Unsupported Expr kind: " << expr.expr_kind_case();
        return false;
    }
  }

 private:
  void makeConstant(CelValue value, Expr* out) {
    auto ident = absl::StrCat("$v", counter_++);
    constant_idents_.emplace(ident, value);
    out->mutable_ident_expr()->set_name(ident);
  }

  CelValue removeConstant(const Expr& ident) {
    return constant_idents_.extract(ident.ident_expr().name()).mapped();
  }

  const CelFunctionRegistry& registry_;

  // Owns constant values created during folding
  google::protobuf::Arena* arena_;
  absl::flat_hash_map<std::string, CelValue>& constant_idents_;

  int counter_;
};

}  // namespace

void FoldConstants(const Expr& expr, const CelFunctionRegistry& registry,
                   google::protobuf::Arena* arena,
                   absl::flat_hash_map<std::string, CelValue>& constant_idents,
                   Expr* out) {
  ConstantFoldingTransform constant_folder(registry, arena, constant_idents);
  constant_folder.Transform(expr, out);
}

}  // namespace google::api::expr::runtime
