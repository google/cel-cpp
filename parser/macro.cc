#include "parser/macro.h"

#include "absl/strings/str_format.h"
#include "common/operators.h"
#include "parser/source_factory.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

using common::CelOperator;

std::string Macro::macroKey() const {
  if (var_arg_style_) {
    return absl::StrFormat("%s:*:%s", function_,
                           receiver_style_ ? "true" : "false");
  } else {
    return absl::StrFormat("%s:%d:%s", function_, arg_count_,
                           receiver_style_ ? "true" : "false");
  }
}

std::vector<Macro> Macro::AllMacros() {
  return {
      // The macro "has(m.f)" which tests the presence of a field, avoiding the
      // need to specify the field as a string.
      Macro(CelOperator::HAS, 1,
            [](std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
               const std::vector<Expr>& args) {
              if (!args.empty() && args[0].has_select_expr()) {
                const auto& sel_expr = args[0].select_expr();
                return sf->newPresenceTestForMacro(macro_id, sel_expr.operand(),
                                                   sel_expr.field());
              } else {
                // error
                return Expr();
              }
            }),

      // The macro "range.all(var, predicate)", which is true if for all
      // elements
      // in range the predicate holds.
      Macro(
          CelOperator::ALL, 2,
          [](std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
             const std::vector<Expr>& args) {
            return sf->newQuantifierExprForMacro(SourceFactory::QUANTIFIER_ALL,
                                                 macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.exists(var, predicate)", which is true if for at least
      // one element in range the predicate holds.
      Macro(
          CelOperator::EXISTS, 2,
          [](std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
             const std::vector<Expr>& args) {
            return sf->newQuantifierExprForMacro(
                SourceFactory::QUANTIFIER_EXISTS, macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.exists_one(var, predicate)", which is true if for
      // exactly one element in range the predicate holds.
      Macro(
          CelOperator::EXISTS_ONE, 2,
          [](std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
             const std::vector<Expr>& args) {
            return sf->newQuantifierExprForMacro(
                SourceFactory::QUANTIFIER_EXISTS_ONE, macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.map(var, function)", applies the function to the vars
      // in
      // the range.
      Macro(
          CelOperator::MAP, 2,
          [](std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
             const std::vector<Expr>& args) {
            return sf->newMapForMacro(macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.map(var, predicate, function)", applies the function
      // to
      // the vars in the range for which the predicate holds true. The other
      // variables are filtered out.
      Macro(
          CelOperator::MAP, 3,
          [](std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
             const std::vector<Expr>& args) {
            return sf->newMapForMacro(macro_id, target, args);
          },
          /* receiver style*/ true),

      // The macro "range.filter(var, predicate)", filters out the variables for
      // which the
      // predicate is false.
      Macro(
          CelOperator::FILTER, 2,
          [](std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
             const std::vector<Expr>& args) {
            return sf->newFilterExprForMacro(macro_id, target, args);
          },
          /* receiver style*/ true),
  };
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
