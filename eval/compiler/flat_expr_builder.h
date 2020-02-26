#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/public/cel_expression.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class FlatExprBuilder : public CelExpressionBuilder {
 public:
  FlatExprBuilder()
      : enable_unknowns_(false),
        enable_unknown_function_results_(false),
        shortcircuiting_(true),
        constant_folding_(false),
        constant_arena_(nullptr),
        enable_comprehension_(true),
        comprehension_max_iterations_(0),
        fail_on_warnings_(true) {}

  // set_enable_unknowns controls support for unknowns in expressions created.
  void set_enable_unknowns(bool enabled) { enable_unknowns_ = enabled; }

  // set_enable_unknown_function_results controls support for unknown function
  // results.
  void set_enable_unknown_function_results(bool enabled) {
    enable_unknown_function_results_ = enabled;
  }

  // set_shortcircuiting regulates shortcircuiting of some expressions.
  // Be default shortcircuiting is enabled.
  void set_shortcircuiting(bool enabled) { shortcircuiting_ = enabled; }

  // Toggle constant folding optimization. By default it is not enabled.
  // The provided arena is used to hold the generated constants.
  void set_constant_folding(bool enabled, google::protobuf::Arena* arena) {
    constant_folding_ = enabled;
    constant_arena_ = arena;
  }

  void set_enable_comprehension(bool enabled) {
    enable_comprehension_ = enabled;
  }

  void set_comprehension_max_iterations(int max_iterations) {
    comprehension_max_iterations_ = max_iterations;
  }

  // Warnings (e.g. no function bound) fail immediately.
  void set_fail_on_warnings(bool should_fail) {
    fail_on_warnings_ = should_fail;
  }

  cel_base::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info) const override;

  cel_base::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info,
      std::vector<absl::Status>* warnings) const override;

 private:
  bool enable_unknowns_;
  bool enable_unknown_function_results_;
  bool shortcircuiting_;

  bool constant_folding_;
  google::protobuf::Arena* constant_arena_;
  bool enable_comprehension_;
  int comprehension_max_iterations_;
  bool fail_on_warnings_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
