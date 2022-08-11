#include "eval/compiler/constant_folding.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "base/ast.h"
#include "eval/eval/const_value_step.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/containers/container_backed_list_impl.h"

namespace cel::ast::internal {

namespace {

using ::google::api::expr::runtime::CelValue;

class ConstantFoldingTransform {
 public:
  ConstantFoldingTransform(
      const google::api::expr::runtime::CelFunctionRegistry& registry,
      google::protobuf::Arena* arena,
      absl::flat_hash_map<std::string, google::api::expr::runtime::CelValue>&
          constant_idents)
      : registry_(registry),
        arena_(arena),
        constant_idents_(constant_idents),
        counter_(0) {}

  // Copies the expression by pulling out constant sub-expressions into
  // CelValue idents. Returns true if the expression is a constant.
  bool Transform(const Expr& expr, Expr* out) {
    out->set_id(expr.id());
    struct {
      ConstantFoldingTransform* transform;
      const Expr& expr;
      Expr* out;

      bool operator()(const Constant& constant) {
        // create a constant that references the input expression data
        // since the output expression is temporary
        auto value = google::api::expr::runtime::ConvertConstant(constant);
        if (value.has_value()) {
          transform->makeConstant(*value, out);
          return true;
        } else {
          out->mutable_const_expr() = expr.const_expr();
          return false;
        }
      }

      bool operator()(const Ident& ident) {
        out->mutable_ident_expr().set_name(expr.ident_expr().name());
        return false;
      }

      bool operator()(const Select& select) {
        auto& select_expr = out->mutable_select_expr();
        transform->Transform(expr.select_expr().operand(),
                             &select_expr.mutable_operand());
        select_expr.set_field(expr.select_expr().field());
        select_expr.set_test_only(expr.select_expr().test_only());
        return false;
      }

      bool operator()(const Call& call) {
        auto& call_expr = out->mutable_call_expr();
        const bool receiver_style = expr.call_expr().has_target();
        const int arg_num = expr.call_expr().args().size();
        bool all_constant = true;
        if (receiver_style) {
          all_constant = transform->Transform(expr.call_expr().target(),
                                              &call_expr.mutable_target()) &&
                         all_constant;
        }
        call_expr.set_function(expr.call_expr().function());
        for (int i = 0; i < arg_num; i++) {
          all_constant =
              transform->Transform(expr.call_expr().args()[i],
                                   &call_expr.mutable_args().emplace_back()) &&
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
        std::vector<CelValue::Type> arg_types(arg_size, CelValue::Type::kAny);
        auto overloads = transform->registry_.FindOverloads(
            call_expr.function(), receiver_style, arg_types);

        // do not proceed if there are no overloads registered
        if (overloads.empty()) {
          return false;
        }

        std::vector<CelValue> arg_values;
        arg_values.reserve(arg_size);
        if (receiver_style) {
          arg_values.push_back(transform->removeConstant(call_expr.target()));
        }
        for (int i = 0; i < arg_num; i++) {
          arg_values.push_back(transform->removeConstant(call_expr.args()[i]));
        }

        // compute function overload
        // consider consolidating the logic with FunctionStep
        const google::api::expr::runtime::CelFunction* matched_function =
            nullptr;
        for (auto overload : overloads) {
          if (overload->MatchArguments(arg_values)) {
            matched_function = overload;
          }
        }
        if (matched_function == nullptr ||
            matched_function->descriptor().is_strict()) {
          // propagate argument errors up the expression
          for (const CelValue& arg : arg_values) {
            if (arg.IsError()) {
              transform->makeConstant(arg, out);
              return true;
            }
          }
        }
        if (matched_function == nullptr) {
          transform->makeConstant(
              google::api::expr::runtime::CreateNoMatchingOverloadError(
                  transform->arena_, call_expr.function()),
              out);
          return true;
        }
        CelValue result;
        auto status =
            matched_function->Evaluate(arg_values, &result, transform->arena_);
        if (status.ok()) {
          transform->makeConstant(result, out);
        } else {
          transform->makeConstant(
              google::api::expr::runtime::CreateErrorValue(
                  transform->arena_, status.message(), status.code()),
              out);
        }
        return true;
      }

      bool operator()(const CreateList& list) {
        auto& list_expr = out->mutable_list_expr();
        int list_size = expr.list_expr().elements().size();
        bool all_constant = true;
        for (int i = 0; i < list_size; i++) {
          auto& elt = list_expr.mutable_elements().emplace_back();
          all_constant =
              transform->Transform(expr.list_expr().elements()[i], &elt) &&
              all_constant;
        }
        if (!all_constant) {
          return false;
        }

        // create a constant list value
        std::vector<CelValue> values(list_size);
        for (int i = 0; i < list_size; i++) {
          values[i] = transform->removeConstant(list_expr.elements()[i]);
        }
        google::api::expr::runtime::CelList* cel_list = google::protobuf::Arena::Create<
            google::api::expr::runtime::ContainerBackedListImpl>(
            transform->arena_, std::move(values));
        transform->makeConstant(CelValue::CreateList(cel_list), out);
        return true;
      }

      bool operator()(const CreateStruct& create_struct) {
        auto& struct_expr = out->mutable_struct_expr();
        struct_expr.set_message_name(expr.struct_expr().message_name());
        int entries_size = expr.struct_expr().entries().size();
        for (int i = 0; i < entries_size; i++) {
          auto& entry = expr.struct_expr().entries()[i];
          auto& new_entry = struct_expr.mutable_entries().emplace_back();
          new_entry.set_id(entry.id());
          struct {
            ConstantFoldingTransform* transform;
            const CreateStruct::Entry& entry;
            CreateStruct::Entry& new_entry;

            void operator()(const std::string& key) {
              new_entry.set_field_key(key);
            }

            void operator()(const std::unique_ptr<Expr>& expr) {
              transform->Transform(entry.map_key(),
                                   &new_entry.mutable_map_key());
            }
          } handler{transform, entry, new_entry};
          absl::visit(handler, entry.key_kind());
          transform->Transform(entry.value(), &new_entry.mutable_value());
        }
        return false;
      }

      bool operator()(const Comprehension& comprehension) {
        // do not fold comprehensions for now: would require significal
        // factoring out of comprehension semantics from the evaluator
        auto& input_expr = expr.comprehension_expr();
        auto& out_expr = out->mutable_comprehension_expr();
        out_expr.set_iter_var(input_expr.iter_var());
        transform->Transform(input_expr.accu_init(),
                             &out_expr.mutable_accu_init());
        transform->Transform(input_expr.iter_range(),
                             &out_expr.mutable_iter_range());
        out_expr.set_accu_var(input_expr.accu_var());
        transform->Transform(input_expr.loop_condition(),
                             &out_expr.mutable_loop_condition());
        transform->Transform(input_expr.loop_step(),
                             &out_expr.mutable_loop_step());
        transform->Transform(input_expr.result(), &out_expr.mutable_result());
        return false;
      }

      bool operator()(absl::monostate) {
        LOG(ERROR) << "Unsupported Expr kind";
        return false;
      }
    } handler{this, expr, out};
    return absl::visit(handler, expr.expr_kind());
  }

  void makeConstant(google::api::expr::runtime::CelValue value, Expr* out) {
    auto ident = absl::StrCat("$v", counter_++);
    constant_idents_.emplace(ident, value);
    out->mutable_ident_expr().set_name(ident);
  }

  google::api::expr::runtime::CelValue removeConstant(const Expr& ident) {
    return constant_idents_.extract(ident.ident_expr().name()).mapped();
  }

 private:
  const google::api::expr::runtime::CelFunctionRegistry& registry_;

  // Owns constant values created during folding
  google::protobuf::Arena* arena_;
  absl::flat_hash_map<std::string, google::api::expr::runtime::CelValue>&
      constant_idents_;

  int counter_;
};

}  // namespace

void FoldConstants(
    const Expr& expr,
    const google::api::expr::runtime::CelFunctionRegistry& registry,
    google::protobuf::Arena* arena,
    absl::flat_hash_map<std::string, CelValue>& constant_idents, Expr* out) {
  ConstantFoldingTransform constant_folder(registry, arena, constant_idents);
  constant_folder.Transform(expr, out);
}

}  // namespace cel::ast::internal
