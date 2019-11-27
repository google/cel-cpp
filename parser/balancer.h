#ifndef THIRD_PARTY_CEL_CPP_PARSER_BALANCER_H_
#define THIRD_PARTY_CEL_CPP_PARSER_BALANCER_H_

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

class SourceFactory;

using google::api::expr::v1alpha1::Expr;

// balancer performs tree balancing on operators whose arguments are of equal
// precedence.
//
// The purpose of the balancer is to ensure a compact serialization format for
// the logical &&, || operators which have a tendency to create long DAGs which
// are skewed in one direction. Since the operators are commutative re-ordering
// the terms *must not* affect the evaluation result.
//
// Based on code from //third_party/cel/go/parser/helper.go
class ExpressionBalancer {
 public:
  ExpressionBalancer(std::shared_ptr<SourceFactory> sf, std::string function,
                     Expr expr);

  // addTerm adds an operation identifier and term to the set of terms to be
  // balanced.
  void addTerm(int64_t op, Expr term);

  // balance creates a balanced tree from the sub-terms and returns the final
  // Expr value.
  Expr balance();

 private:
  // balancedTree recursively balances the terms provided to a commutative
  // operator.
  Expr balancedTree(int lo, int hi);

 private:
  std::shared_ptr<SourceFactory> sf_;
  std::string function_;
  std::vector<Expr> terms_;
  std::vector<int64_t> ops_;
};

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_BALANCER_H_
