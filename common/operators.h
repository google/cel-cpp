#ifndef THIRD_PARTY_CEL_CPP_COMMON_OPERATORS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_OPERATORS_H_

#include <map>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/strings/string_view.h"

namespace google {
namespace api {
namespace expr {
namespace common {

// Operator function names.
struct CelOperator {
  static constexpr const char* CONDITIONAL = "_?_:_";
  static constexpr const char* LOGICAL_AND = "_&&_";
  static constexpr const char* LOGICAL_OR = "_||_";
  static constexpr const char* LOGICAL_NOT = "!_";
  static constexpr const char* IN_DEPRECATED = "_in_";
  static constexpr const char* EQUALS = "_==_";
  static constexpr const char* NOT_EQUALS = "_!=_";
  static constexpr const char* LESS = "_<_";
  static constexpr const char* LESS_EQUALS = "_<=_";
  static constexpr const char* GREATER = "_>_";
  static constexpr const char* GREATER_EQUALS = "_>=_";
  static constexpr const char* ADD = "_+_";
  static constexpr const char* SUBTRACT = "_-_";
  static constexpr const char* MULTIPLY = "_*_";
  static constexpr const char* DIVIDE = "_/_";
  static constexpr const char* MODULO = "_%_";
  static constexpr const char* NEGATE = "-_";
  static constexpr const char* INDEX = "_[_]";
  // Macros
  static constexpr const char* HAS = "has";
  static constexpr const char* ALL = "all";
  static constexpr const char* EXISTS = "exists";
  static constexpr const char* EXISTS_ONE = "exists_one";
  static constexpr const char* MAP = "map";
  static constexpr const char* FILTER = "filter";

  // Named operators, must not have be valid identifiers.
  static constexpr const char* NOT_STRICTLY_FALSE = "@not_strictly_false";
  static constexpr const char* IN = "@in";
};

// These give access to all or some specific precedence value.
// Higher value means higher precedence, 0 means no precedence, i.e.,
// custom function and not builtin operator.
int LookupPrecedence(const std::string& op);

std::optional<std::string> LookupUnaryOperator(const std::string& op);
std::optional<std::string> LookupBinaryOperator(const std::string& op);
std::optional<std::string> LookupOperator(const std::string& op);
std::optional<std::string> ReverseLookupOperator(const std::string& op);

// returns true if op has a lower precedence than the one expressed in expr
bool IsOperatorLowerPrecedence(const std::string& op, const Expr& expr);
// returns true if op has the same precedence as the one expressed in expr
bool IsOperatorSamePrecedence(const std::string& op, const Expr& expr);
// return true if operator is left recursive, i.e., neither && nor ||.
bool IsOperatorLeftRecursive(const std::string& op);

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_OPERATORS_H_
