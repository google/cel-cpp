#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_

#include <functional>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/strings/string_view.h"
#include "eval/public/activation.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelEvaluationListener is the callback that is passed to (and called by)
// CelEvaluation::Trace. It gets an expression node ID from the original
// expression, its value and the arena object. If an expression node
// is evaluated multiple times (e.g. as a part of Comprehension.loop_step)
// then the order of the callback invocations is guaranteed to correspond
// the order of variable sub-elements (e.g. the order of elements returned
// by Comprehension.iter_range).
using CelEvaluationListener = std::function<absl::Status(
    int64_t expr_id, const CelValue&, google::protobuf::Arena*)>;

// An opaque state used for evaluation of a cell expression.
class CelEvaluationState {
 public:
  virtual ~CelEvaluationState() = default;
};

// Base interface for expression evaluating objects.
class CelExpression {
 public:
  virtual ~CelExpression() = default;

  // Initializes the state
  virtual std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const = 0;

  // Evaluates expression and returns value.
  // activation contains bindings from parameter names to values
  // arena parameter specifies Arena object where output result and
  // internal data will be allocated.
  virtual cel_base::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                            google::protobuf::Arena* arena) const = 0;

  // Evaluates expression and returns value.
  // activation contains bindings from parameter names to values
  // state must be non-null and created prior to calling Evaluate by
  // InitializeState.
  virtual cel_base::StatusOr<CelValue> Evaluate(
      const BaseActivation& activation, CelEvaluationState* state) const = 0;

  // Trace evaluates expression calling the callback on each sub-tree.
  virtual cel_base::StatusOr<CelValue> Trace(
      const BaseActivation& activation, google::protobuf::Arena* arena,
      CelEvaluationListener callback) const = 0;

  // Trace evaluates expression calling the callback on each sub-tree.
  // state must be non-null and created prior to calling Evaluate by
  // InitializeState.
  virtual cel_base::StatusOr<CelValue> Trace(
      const BaseActivation& activation, CelEvaluationState* state,
      CelEvaluationListener callback) const = 0;
};

// Base class for Expression Builder implementations
// Provides user with factory to register extension functions.
// ExpressionBuilder MUST NOT be destroyed before CelExpression objects
// it built.
class CelExpressionBuilder {
 public:
  CelExpressionBuilder()
      : registry_(absl::make_unique<CelFunctionRegistry>()) {}

  virtual ~CelExpressionBuilder() {}

  // Creates CelExpression object from AST tree.
  // expr specifies root of AST tree
  virtual cel_base::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info) const = 0;

  // Creates CelExpression object from AST tree.
  // expr specifies root of AST tree.
  // non-fatal build warnings are written to warnings if encountered.
  virtual cel_base::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info,
      std::vector<absl::Status>* warnings) const = 0;

  // CelFunction registry. Extension function should be registered with it
  // prior to expression creation.
  CelFunctionRegistry* GetRegistry() const { return registry_.get(); }

  // Enums registered with the builder.
  const std::set<const google::protobuf::EnumDescriptor*>& resolvable_enums() const {
    return resolvable_enums_;
  }

  // Add Enum to the list of resolvable by the builder.
  void AddResolvableEnum(const google::protobuf::EnumDescriptor* enum_descriptor) {
    resolvable_enums_.emplace(enum_descriptor);
  }

  // Remove Enum from the list of resolvable by the builder.
  void RemoveResolvableEnum(const google::protobuf::EnumDescriptor* enum_descriptor) {
    resolvable_enums_.erase(enum_descriptor);
  }

  void set_container(std::string container) {
    container_ = std::move(container);
  }

  absl::string_view container() const { return container_; }

 private:
  std::unique_ptr<CelFunctionRegistry> registry_;
  std::set<const google::protobuf::EnumDescriptor*> resolvable_enums_;
  std::string container_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_
