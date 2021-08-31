#ifndef THIRD_PARTY_CEL_CPP_PARSER_OPTIONS_H_
#define THIRD_PARTY_CEL_CPP_PARSER_OPTIONS_H_

namespace google {
namespace api {
namespace expr {
namespace parser {

inline constexpr int kDefaultErrorRecoveryLimit = 30;
inline constexpr int kDefaultMaxRecursionDepth = 250;
inline constexpr int kExpressionSizeCodepointLimit = 100'000;
inline constexpr int kDefaultErrorRecoveryTokenLookaheadLimit = 512;
inline constexpr bool kDefaultAddMacroCalls = false;

// Options for configuring the limits and features of the parser.
struct ParserOptions {
  // Limit of the number of error recovery attempts made by the ANTLR parser
  // when processing an input. This limit, when reached, will halt further
  // parsing of the expression.
  int error_recovery_limit = kDefaultErrorRecoveryLimit;

  // Limit on the amount of recusive parse instructions permitted when building
  // the abstract syntax tree for the expression. This prevents pathological
  // inputs from causing stack overflows.
  int max_recursion_depth = kDefaultMaxRecursionDepth;

  // Limit on the number of codepoints in the input string which the parser will
  // attempt to parse.
  int expression_size_codepoint_limit = kExpressionSizeCodepointLimit;

  // Limit on the number of lookahead tokens to consume when attempting to
  // recover from an error.
  int error_recovery_token_lookahead_limit =
      kDefaultErrorRecoveryTokenLookaheadLimit;

  // Add macro calls to macro_calls list in source_info.
  bool add_macro_calls = kDefaultAddMacroCalls;
};

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_OPTIONS_H_
