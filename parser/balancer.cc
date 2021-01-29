#include "parser/balancer.h"

#include "parser/source_factory.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

ExpressionBalancer::ExpressionBalancer(std::shared_ptr<SourceFactory> sf,
                                       std::string function, Expr expr)
    : sf_(std::move(sf)),
      function_(std::move(function)),
      terms_{std::move(expr)},
      ops_{} {}

void ExpressionBalancer::addTerm(int64_t op, Expr term) {
  terms_.push_back(std::move(term));
  ops_.push_back(op);
}

Expr ExpressionBalancer::balance() {
  if (terms_.size() == 1) {
    return terms_[0];
  }
  return balancedTree(0, ops_.size() - 1);
}

Expr ExpressionBalancer::balancedTree(int lo, int hi) {
  int mid = (lo + hi + 1) / 2;

  Expr left;
  if (mid == lo) {
    left = terms_[mid];
  } else {
    left = balancedTree(lo, mid - 1);
  }

  Expr right;
  if (mid == hi) {
    right = terms_[mid + 1];
  } else {
    right = balancedTree(mid + 1, hi);
  }
  return sf_->newGlobalCall(ops_[mid], function_,
                            {std::move(left), std::move(right)});
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
