#ifndef THIRD_PARTY_CEL_CPP_COMMON_CEL_OPERATORS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CEL_OPERATORS_H_

namespace google {
namespace api {
namespace expr {

class Operators {
 public:
  static constexpr char CONDITIONAL_NAME[] = "_?_:_";
  static constexpr char LOGICAL_AND_NAME[] = "_&&_";
  static constexpr char LOGICAL_OR_NAME[] = "_||_";
  static constexpr char LOGICAL_NOT_NAME[] = "!_";
  static constexpr char IN_NAME[] = "_in_";
  static constexpr char EQUALS_NAME[] = "_==_";
  static constexpr char NOT_EQUALS_NAME[] = "_!=_";
  static constexpr char LESS_NAME[] = "_<_";
  static constexpr char LESS_EQUALS_NAME[] = "_<=_";
  static constexpr char GREATER_NAME[] = "_>_";
  static constexpr char GREATER_EQUALS_NAME[] = "_>=_";
  static constexpr char ADD_NAME[] = "_+_";
  static constexpr char SUBTRACT_NAME[] = "_-_";
  static constexpr char MULTIPLY_NAME[] = "_*_";
  static constexpr char DIVIDE_NAME[] = "_/_";
  static constexpr char MODULO_NAME[] = "_%_";
  static constexpr char NEGATE_NAME[] = "-_";
  static constexpr char INDEX_NAME[] = "_[_]";
  static constexpr char HAS_NAME[] = "has";
  static constexpr char ALL_NAME[] = "all";
  static constexpr char EXISTS_NAME[] = "exists";
  static constexpr char EXISTS_ONE_NAME[] = "exists_one";
  static constexpr char MAP_NAME[] = "map";
  static constexpr char FILTER_NAME[] = "filter";
};

}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_CEL_OPERATORS_H_
