#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::v1alpha1::CheckedExpr;
using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;
using testing::HasSubstr;
using cel::internal::StatusIs;

// [1, 2].filter(x, [3, 4].all(y, x < y))
const char kNestedComprehension[] = R"pb(
  id: 27
  comprehension_expr {
    iter_var: "x"
    iter_range {
      id: 1
      list_expr {
        elements {
          id: 2
          const_expr { int64_value: 1 }
        }
        elements {
          id: 3
          const_expr { int64_value: 2 }
        }
      }
    }
    accu_var: "__result__"
    accu_init {
      id: 22
      list_expr {}
    }
    loop_condition {
      id: 23
      const_expr { bool_value: true }
    }
    loop_step {
      id: 26
      call_expr {
        function: "_?_:_"
        args {
          id: 20
          comprehension_expr {
            iter_var: "y"
            iter_range {
              id: 6
              list_expr {
                elements {
                  id: 7
                  const_expr { int64_value: 3 }
                }
                elements {
                  id: 8
                  const_expr { int64_value: 4 }
                }
              }
            }
            accu_var: "__result__"
            accu_init {
              id: 14
              const_expr { bool_value: true }
            }
            loop_condition {
              id: 16
              call_expr {
                function: "@not_strictly_false"
                args {
                  id: 15
                  ident_expr { name: "__result__" }
                }
              }
            }
            loop_step {
              id: 18
              call_expr {
                function: "_&&_"
                args {
                  id: 17
                  ident_expr { name: "__result__" }
                }
                args {
                  id: 12
                  call_expr {
                    function: "_<_"
                    args {
                      id: 11
                      ident_expr { name: "x" }
                    }
                    args {
                      id: 13
                      ident_expr { name: "y" }
                    }
                  }
                }
              }
            }
            result {
              id: 19
              ident_expr { name: "__result__" }
            }
          }
        }
        args {
          id: 25
          call_expr {
            function: "_+_"
            args {
              id: 21
              ident_expr { name: "__result__" }
            }
            args {
              id: 24
              list_expr {
                elements {
                  id: 5
                  ident_expr { name: "x" }
                }
              }
            }
          }
        }
        args {
          id: 21
          ident_expr { name: "__result__" }
        }
      }
    }
    result {
      id: 21
      ident_expr { name: "__result__" }
    }
  })pb";

TEST(FlatExprBuilderComprehensionsTest, NestedComp) {
  FlatExprBuilder builder;
  Expr expr;
  SourceInfo source_info;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kNestedComprehension, &expr));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
}

TEST(FlatExprBuilderComprehensionsTest, InvalidComprehensionWithRewrite) {
  CheckedExpr expr;
  // The rewrite step which occurs when an identifier gets a more qualified name
  // from the reference map has the potential to make invalid comprehensions
  // appear valid, by populating missing fields with default values.
  // var.<macro>(x, <missing>)
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        reference_map {
                                          key: 1
                                          value { name: "qualified.var" }
                                        }
                                        expr {
                                          comprehension_expr {
                                            iter_var: "x"
                                            iter_range {
                                              id: 1
                                              ident_expr { name: "var" }
                                            }
                                            accu_var: "y"
                                            accu_init {
                                              id: 1
                                              const_expr { bool_value: true }
                                            }
                                          }
                                        })pb",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid comprehension")));
}

}  // namespace

}  // namespace google::api::expr::runtime
