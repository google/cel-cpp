#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_EXPR_PRINTER_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_EXPR_PRINTER_H_

#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace testutil {

using ::google::api::expr::v1alpha1::Expr;

class ExpressionAdorner {
 public:
  virtual ~ExpressionAdorner() {}
  virtual std::string adorn(const Expr& e) const = 0;
  virtual std::string adorn(const Expr::CreateStruct::Entry& e) const = 0;
};

const ExpressionAdorner& empty_adorner();

class ExprPrinter {
 public:
  ExprPrinter() : adorner_(empty_adorner()) {}
  ExprPrinter(const ExpressionAdorner& adorner) : adorner_(adorner) {}
  std::string print(const Expr& expr) const;

 private:
  const ExpressionAdorner& adorner_;
};

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_EXPR_PRINTER_H_
