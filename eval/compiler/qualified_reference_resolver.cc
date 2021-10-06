#include "eval/compiler/qualified_reference_resolver.h"

#include <cstdint>
#include <functional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::Constant;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::Reference;

// Determines if function is implemented with custom evaluation step instead of
// registered.
bool IsSpecialFunction(absl::string_view function_name) {
  return function_name == builtin::kAnd || function_name == builtin::kOr ||
         function_name == builtin::kIndex || function_name == builtin::kTernary;
}

// Convert a select expr sub tree into a namespace name if possible.
// If any operand of the top element is a not a select or an ident node,
// return nullopt.
absl::optional<std::string> ToNamespace(const Expr& expr) {
  absl::optional<std::string> maybe_parent_namespace;
  switch (expr.expr_kind_case()) {
    case Expr::kIdentExpr:
      return expr.ident_expr().name();
    case Expr::kSelectExpr:
      if (expr.select_expr().test_only()) {
        return absl::nullopt;
      }
      maybe_parent_namespace = ToNamespace(expr.select_expr().operand());
      if (!maybe_parent_namespace.has_value()) {
        return absl::nullopt;
      }
      return absl::StrCat(*maybe_parent_namespace, ".",
                          expr.select_expr().field());
    default:
      return absl::nullopt;
  }
}

bool OverloadExists(const Resolver& resolver, absl::string_view name,
                    const std::vector<CelValue::Type>& arguments_matcher,
                    bool receiver_style = false) {
  return !resolver.FindOverloads(name, receiver_style, arguments_matcher)
              .empty() ||
         !resolver.FindLazyOverloads(name, receiver_style, arguments_matcher)
              .empty();
}

// Return the qualified name of the most qualified matching overload, or
// nullopt if no matches are found.
absl::optional<std::string> BestOverloadMatch(const Resolver& resolver,
                                              absl::string_view base_name,
                                              int argument_count) {
  if (IsSpecialFunction(base_name)) {
    return std::string(base_name);
  }
  auto arguments_matcher = ArgumentsMatcher(argument_count);
  // Check from most qualified to least qualified for a matching overload.
  auto names = resolver.FullyQualifiedNames(base_name);
  for (auto name = names.begin(); name != names.end(); ++name) {
    if (OverloadExists(resolver, *name, arguments_matcher)) {
      return *name;
    }
  }
  return absl::nullopt;
}

class ReferenceResolver {
 public:
  ReferenceResolver(const google::protobuf::Map<int64_t, Reference>& reference_map,
                    const Resolver& resolver, BuilderWarnings* warnings)
      : reference_map_(reference_map),
        resolver_(resolver),
        warnings_(warnings) {}

  // Attempt to resolve references in expr. Return true if part of the
  // expression was rewritten.
  // TODO(issues/95): If possible, it would be nice to write a general utility
  // for running the preprocess steps when traversing the AST instead of having
  // one pass per transform.
  absl::StatusOr<bool> Rewrite(Expr* out) {
    const auto reference_iter = reference_map_.find(out->id());
    const Reference* reference = nullptr;
    if (reference_iter != reference_map_.end()) {
      if (!reference_iter->second.has_value()) {
        reference = &reference_iter->second;
      } else {
        if (out->expr_kind_case() == Expr::kIdentExpr &&
            reference_iter->second.value().constant_kind_case() ==
                Constant::kInt64Value) {
          // Replace enum idents with const reference value.
          out->clear_ident_expr();
          out->mutable_const_expr()->set_int64_value(
              reference_iter->second.value().int64_value());
          return true;
        }
      }
    }
    bool updated = false;

    switch (out->expr_kind_case()) {
      case Expr::kConstExpr: {
        return false;
      }
      case Expr::kIdentExpr:
        return MaybeUpdateIdentNode(out, reference);
      case Expr::kSelectExpr:
        return MaybeUpdateSelectNode(out, reference);
      case Expr::kCallExpr: {
        return MaybeUpdateCallNode(out, reference);
      }
      case Expr::kListExpr: {
        auto* list_expr = out->mutable_list_expr();
        int list_size = list_expr->elements_size();
        for (int i = 0; i < list_size; i++) {
          CEL_ASSIGN_OR_RETURN(bool rewrite_result,
                               Rewrite(list_expr->mutable_elements(i)));
          updated = updated || rewrite_result;
        }
        return updated;
      }
      case Expr::kStructExpr: {
        return MaybeUpdateStructNode(out, reference);
      }
      case Expr::kComprehensionExpr: {
        auto* out_expr = out->mutable_comprehension_expr();
        bool rewrite_result;

        if (out_expr->has_accu_init()) {
          CEL_ASSIGN_OR_RETURN(rewrite_result,
                               Rewrite(out_expr->mutable_accu_init()));
          updated = updated || rewrite_result;
        }

        if (out_expr->has_iter_range()) {
          CEL_ASSIGN_OR_RETURN(rewrite_result,
                               Rewrite(out_expr->mutable_iter_range()));
          updated = updated || rewrite_result;
        }

        if (out_expr->has_loop_condition()) {
          CEL_ASSIGN_OR_RETURN(rewrite_result,
                               Rewrite(out_expr->mutable_loop_condition()));
          updated = updated || rewrite_result;
        }

        if (out_expr->has_loop_step()) {
          CEL_ASSIGN_OR_RETURN(rewrite_result,
                               Rewrite(out_expr->mutable_loop_step()));
          updated = updated || rewrite_result;
        }

        if (out_expr->has_result()) {
          CEL_ASSIGN_OR_RETURN(rewrite_result,
                               Rewrite(out_expr->mutable_result()));
          updated = updated || rewrite_result;
        }

        return updated;
      }
      default:
        GOOGLE_LOG(ERROR) << "Unsupported Expr kind: " << out->expr_kind_case();
        return false;
    }
  }

 private:
  // Attempt to update a function call node. This disambiguates
  // receiver call verses namespaced names in parse if possible.
  //
  // TODO(issues/95): This duplicates some of the overload matching behavior
  // for parsed expressions. We should refactor to consolidate the code.
  absl::StatusOr<bool> MaybeUpdateCallNode(Expr* out,
                                           const Reference* reference) {
    auto* call_expr = out->mutable_call_expr();
    if (reference != nullptr && reference->overload_id_size() == 0) {
      CEL_RETURN_IF_ERROR(warnings_->AddWarning(absl::InvalidArgumentError(
          absl::StrCat("Reference map doesn't provide overloads for ",
                       out->call_expr().function()))));
    }
    bool receiver_style = call_expr->has_target();
    bool updated = false;
    int arg_num = call_expr->args_size();
    if (receiver_style) {
      // First check the target to see if the reference map indicates it
      // should be rewritten.
      absl::StatusOr<bool> rewrite_result =
          Rewrite(call_expr->mutable_target());
      CEL_RETURN_IF_ERROR(rewrite_result.status());
      bool target_updated = rewrite_result.value();
      updated = target_updated;
      if (!target_updated) {
        // If the function receiver was not rewritten, check to see if it's
        // actually a namespace for the function.
        auto maybe_namespace = ToNamespace(call_expr->target());
        if (maybe_namespace.has_value()) {
          std::string resolved_name =
              absl::StrCat(*maybe_namespace, ".", call_expr->function());
          auto maybe_resolved_function =
              BestOverloadMatch(resolver_, resolved_name, arg_num);
          if (maybe_resolved_function.has_value()) {
            call_expr->set_function(maybe_resolved_function.value());
            call_expr->clear_target();
            updated = true;
          }
        }
      }
    } else {
      // Not a receiver style function call. Check to see if it is a namespaced
      // function using a shorthand inside the expression container.
      auto maybe_resolved_function =
          BestOverloadMatch(resolver_, call_expr->function(), arg_num);
      if (!maybe_resolved_function.has_value()) {
        CEL_RETURN_IF_ERROR(warnings_->AddWarning(absl::InvalidArgumentError(
            absl::StrCat("No overload found in reference resolve step for ",
                         call_expr->function()))));
      } else if (maybe_resolved_function.value() != call_expr->function()) {
        call_expr->set_function(maybe_resolved_function.value());
        updated = true;
      }
    }
    // For parity, if we didn't rewrite the receiver call style function,
    // check that an overload is provided in the builder.
    if (call_expr->has_target() &&
        !OverloadExists(resolver_, call_expr->function(),
                        ArgumentsMatcher(arg_num + 1),
                        /* receiver_style= */ true)) {
      CEL_RETURN_IF_ERROR(warnings_->AddWarning(absl::InvalidArgumentError(
          absl::StrCat("No overload found in reference resolve step for ",
                       call_expr->function()))));
    }
    for (int i = 0; i < arg_num; i++) {
      absl::StatusOr<bool> rewrite_result = Rewrite(call_expr->mutable_args(i));
      CEL_RETURN_IF_ERROR(rewrite_result.status());
      updated = updated || rewrite_result.value();
    }
    return updated;
  }

  // Attempt to resolve a select node. If reference is not-null and valid,
  // replace the select node with the fully qualified ident node. Otherwise,
  // continue recursively rewriting the Expr.
  absl::StatusOr<bool> MaybeUpdateSelectNode(Expr* out,
                                             const Reference* reference) {
    if (reference != nullptr) {
      if (out->select_expr().test_only()) {
        CEL_RETURN_IF_ERROR(warnings_->AddWarning(
            absl::InvalidArgumentError("Reference map points to a presence "
                                       "test -- has(container.attr)")));
      } else if (!reference->name().empty()) {
        out->clear_select_expr();
        out->mutable_ident_expr()->set_name(reference->name());
        return true;
      }
    }
    return Rewrite(out->mutable_select_expr()->mutable_operand());
  }

  // Attempt to resolve an ident node. If reference is not-null and valid,
  // replace the node with the fully qualified ident node.
  bool MaybeUpdateIdentNode(Expr* out, const Reference* reference) {
    if (reference != nullptr && !reference->name().empty() &&
        reference->name() != out->ident_expr().name()) {
      out->mutable_ident_expr()->set_name(reference->name());
      return true;
    }
    return false;
  }

  // Update a create struct node. Currently, just handles recursing.
  //
  // TODO(issues/72): annotating the execution plan with this may help
  // identify problems with the environment setup. This will probably
  // also require the type map information from a checked expression.
  absl::StatusOr<bool> MaybeUpdateStructNode(Expr* out,
                                             const Reference* reference) {
    auto* struct_expr = out->mutable_struct_expr();
    int entries_size = struct_expr->entries_size();
    bool updated = false;
    for (int i = 0; i < entries_size; i++) {
      auto* new_entry = struct_expr->mutable_entries(i);
      switch (new_entry->key_kind_case()) {
        case Expr::CreateStruct::Entry::kFieldKey:
          // Nothing to do.
          break;
        case Expr::CreateStruct::Entry::kMapKey: {
          auto key_updated = Rewrite(new_entry->mutable_map_key());
          CEL_RETURN_IF_ERROR(key_updated.status());
          updated = updated || key_updated.value();
          break;
        }
        default:
          GOOGLE_LOG(ERROR) << "Unsupported Entry kind: "
                     << new_entry->key_kind_case();
          break;
      }
      auto value_updated = Rewrite(new_entry->mutable_value());
      CEL_RETURN_IF_ERROR(value_updated.status());
      updated = updated || value_updated.value();
    }
    return updated;
  }

  const google::protobuf::Map<int64_t, Reference>& reference_map_;
  const Resolver& resolver_;
  BuilderWarnings* warnings_;
};

}  // namespace

absl::StatusOr<absl::optional<Expr>> ResolveReferences(
    const Expr& expr, const google::protobuf::Map<int64_t, Reference>& reference_map,
    const Resolver& resolver, BuilderWarnings* warnings) {
  Expr out(expr);
  ReferenceResolver ref_resolver(reference_map, resolver, warnings);
  absl::StatusOr<bool> rewrite_result = ref_resolver.Rewrite(&out);
  if (!rewrite_result.ok()) {
    return rewrite_result.status();
  } else if (rewrite_result.value()) {
    return absl::optional<Expr>(out);
  } else {
    return absl::optional<Expr>();
  }
}

}  // namespace google::api::expr::runtime
