#include "eval/compiler/qualified_reference_resolver.h"

#include "absl/status/status.h"
#include "base/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::Reference;

class ReferenceResolver {
 public:
  ReferenceResolver(const google::protobuf::Map<int64_t, Reference>& reference_map,
                    BuilderWarnings* warnings)
      : reference_map_(reference_map), warnings_(warnings) {}

  // Attempt to resolve references in expr. Return true if part of the
  // expression was rewritten.
  cel_base::StatusOr<bool> Rewrite(Expr* out) {
    cel_base::StatusOr<bool> maybe_rewrite_result = MaybeRewriteReferences(out);
    RETURN_IF_ERROR(maybe_rewrite_result.status());

    if (maybe_rewrite_result.value()) {
      return true;
    }
    // If we don't have a rewrite rule, continue traversing the AST.
    switch (out->expr_kind_case()) {
      case Expr::kConstExpr: {
        return false;
      }
      case Expr::kIdentExpr:
        return false;
      case Expr::kSelectExpr: {
        return Rewrite(out->mutable_select_expr()->mutable_operand());
      }
      case Expr::kCallExpr: {
        auto* call_expr = out->mutable_call_expr();
        const bool receiver_style = call_expr->has_target();
        const int arg_num = call_expr->args_size();
        bool args_updated = false;
        if (receiver_style) {
          cel_base::StatusOr<bool> rewrite_result =
              Rewrite(call_expr->mutable_target());
          RETURN_IF_ERROR(rewrite_result.status());
          args_updated = args_updated || rewrite_result.value();
        }
        for (int i = 0; i < arg_num; i++) {
          cel_base::StatusOr<bool> rewrite_result =
              Rewrite(call_expr->mutable_args(i));
          RETURN_IF_ERROR(rewrite_result.status());
          args_updated = args_updated || rewrite_result.value();
        }
        return args_updated;
      }
      case Expr::kListExpr: {
        auto* list_expr = out->mutable_list_expr();
        int list_size = list_expr->elements_size();
        bool args_updated = false;
        for (int i = 0; i < list_size; i++) {
          cel_base::StatusOr<bool> rewrite_result =
              Rewrite(list_expr->mutable_elements(i));
          RETURN_IF_ERROR(rewrite_result.status());
          args_updated = args_updated || rewrite_result.value();
        }
        return args_updated;
      }
      case Expr::kStructExpr: {
        auto* struct_expr = out->mutable_struct_expr();
        int entries_size = struct_expr->entries_size();
        bool args_updated = false;
        for (int i = 0; i < entries_size; i++) {
          auto* new_entry = struct_expr->mutable_entries(i);
          switch (new_entry->key_kind_case()) {
            case Expr::CreateStruct::Entry::kFieldKey:
              // Nothing to do.
              break;
            case Expr::CreateStruct::Entry::kMapKey:
              args_updated =
                  Rewrite(new_entry->mutable_map_key()).ok() || args_updated;
              break;
            default:
              GOOGLE_LOG(ERROR) << "Unsupported Entry kind: "
                         << new_entry->key_kind_case();
              break;
          }
          args_updated =
              Rewrite(new_entry->mutable_value()).ok() || args_updated;
        }
        return args_updated;
      }
      case Expr::kComprehensionExpr: {
        auto* out_expr = out->mutable_comprehension_expr();
        bool args_updated = false;
        cel_base::StatusOr<bool> rewrite_result;

        rewrite_result = Rewrite(out_expr->mutable_accu_init());
        RETURN_IF_ERROR(rewrite_result.status());
        args_updated = args_updated || rewrite_result.value();

        rewrite_result = Rewrite(out_expr->mutable_iter_range());
        RETURN_IF_ERROR(rewrite_result.status());
        args_updated = args_updated || rewrite_result.value();

        rewrite_result = Rewrite(out_expr->mutable_loop_condition());
        RETURN_IF_ERROR(rewrite_result.status());
        args_updated = args_updated || rewrite_result.value();

        rewrite_result = Rewrite(out_expr->mutable_loop_step());
        RETURN_IF_ERROR(rewrite_result.status());
        args_updated = args_updated || rewrite_result.value();

        rewrite_result = Rewrite(out_expr->mutable_result());
        RETURN_IF_ERROR(rewrite_result.status());
        args_updated = args_updated || rewrite_result.value();

        return args_updated;
      }
      default:
        GOOGLE_LOG(ERROR) << "Unsupported Expr kind: " << out->expr_kind_case();
        return false;
    }
  }

 private:
  // Attempts to apply rewrites for reference map. Returns true if rewrites
  // occur.
  cel_base::StatusOr<bool> MaybeRewriteReferences(Expr* expr) {
    const auto iter = reference_map_.find(expr->id());
    if (iter == reference_map_.end()) {
      return false;
    }
    const Reference& reference = iter->second;
    if (reference.has_value() || !reference.overload_id().empty()) {
      // TODO(issues/71): Add support for functions and compile time
      // constants.
      return false;
    }

    switch (expr->expr_kind_case()) {
      case Expr::ExprKindCase::kIdentExpr:
        if (reference.name() != expr->ident_expr().name()) {
          // Possibly shorthand for a namespaced name.
          expr->clear_ident_expr();
          expr->mutable_ident_expr()->set_name(reference.name());
          return true;
        } else {
          return false;
        }
      case Expr::ExprKindCase::kStructExpr:
        // reference to a create struct message type. nothing to do.
        // TODO(issues/72): annotating the execution plan with this may help
        // identify problems with the environment setup. This will probably
        // also require the type map information from a checked expression.
        return false;
      case Expr::ExprKindCase::kSelectExpr:
        if (expr->select_expr().test_only()) {
          RETURN_IF_ERROR(warnings_->AddWarning(
              absl::InvalidArgumentError("Reference map points to a presence "
                                         "test -- has(container.attr)")));
          return false;
        }
        expr->clear_select_expr();
        expr->mutable_ident_expr()->set_name(reference.name());
        return true;
      default:
        RETURN_IF_ERROR(
            warnings_->AddWarning(absl::InvalidArgumentError(absl::StrCat(
                "Unsupported reference kind: ", expr->expr_kind_case()))));
        return false;
    }
  }

  const google::protobuf::Map<int64_t, Reference>& reference_map_;
  BuilderWarnings* warnings_;
};

}  // namespace

cel_base::StatusOr<absl::optional<Expr>> ResolveReferences(
    const Expr& expr, const google::protobuf::Map<int64_t, Reference>& reference_map,
    BuilderWarnings* warnings) {
  Expr out(expr);
  ReferenceResolver resolver(reference_map, warnings);
  cel_base::StatusOr<bool> rewrite_result = resolver.Rewrite(&out);
  if (!rewrite_result.ok()) {
    return rewrite_result.status();
  } else if (rewrite_result.value()) {
    return absl::optional<Expr>(out);
  } else {
    return absl::optional<Expr>();
  }
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
