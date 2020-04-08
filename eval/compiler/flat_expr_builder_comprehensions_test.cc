#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;

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
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kNestedComprehension, &expr));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  SourceInfo source_info;
  auto build_status = builder.CreateExpression(&expr, &source_info);
  ASSERT_OK(build_status);

  auto cel_expr = std::move(build_status.value());

  Activation activation;
  google::protobuf::Arena arena;
  auto result_or = cel_expr->Evaluate(activation, &arena);
  ASSERT_OK(result_or);
  CelValue result = result_or.value();
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
