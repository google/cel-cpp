#ifndef THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_
#define THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_

#include <functional>
#include <memory>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

using google::api::expr::v1alpha1::Expr;

class SourceFactory;

// MacroExpander converts the target and args of a function call that matches a
// Macro.
//
// Note: when the Macros.IsReceiverStyle() is true, the target argument will be
// empty.
using MacroExpander =
    std::function<Expr(std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr*,
                       const std::vector<Expr>&)>;

// Macro interface for describing the function signature to match and the
// MacroExpander to apply.
//
// Note: when a Macro should apply to multiple overloads (based on arg count) of
// a given function, a Macro should be created per arg-count.
class Macro {
 public:
  // Create a Macro for a global function with the specified number of arguments
  Macro(const std::string& function, int arg_count, MacroExpander expander,
        bool receiver_style = false)
      : function_(function),
        receiver_style_(receiver_style),
        var_arg_style_(false),
        arg_count_(arg_count),
        expander_(expander) {}

  Macro(const std::string& function, MacroExpander expander,
        bool receiver_style = false)
      : function_(function),
        receiver_style_(receiver_style),
        var_arg_style_(true),
        arg_count_(0),
        expander_(expander) {}

  // Function name to match.
  std::string function() const { return function_; }

  // ArgCount for the function call.
  //
  // When the macro is a var-arg style macro, the return value will be zero, but
  // the MacroKey will contain a `*` where the arg count would have been.
  int argCount() const { return arg_count_; }

  // IsReceiverStyle returns true if the macro matches a receiver style call.
  bool isReceiverStyle() const { return receiver_style_; }

  // MacroKey returns the macro signatures accepted by this macro.
  //
  // Format: `<function>:<arg-count>:<is-receiver>`.
  //
  // When the macros is a var-arg style macro, the `arg-count` value is
  // represented as a `*`.
  std::string macroKey() const;

  // Expander returns the MacroExpander to apply when the macro key matches the
  // parsed call signature.
  const MacroExpander& expander() const { return expander_; }

  Expr expand(std::shared_ptr<SourceFactory> sf, int64_t macro_id, Expr* target,
              const std::vector<Expr>& args) {
    return expander_(sf, macro_id, target, args);
  }

  static std::vector<Macro> AllMacros();

 private:
  std::string function_;
  bool receiver_style_;
  bool var_arg_style_;
  int arg_count_;
  MacroExpander expander_;
};

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_
