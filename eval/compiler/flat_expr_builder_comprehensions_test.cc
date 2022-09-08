/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/arena.h"
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
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/testing/matchers.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::v1alpha1::CheckedExpr;
using testing::HasSubstr;
using cel::internal::StatusIs;

TEST(FlatExprBuilderComprehensionsTest, NestedComp) {
  FlatExprBuilder builder;
  builder.set_enable_comprehension_list_append(true);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       parser::Parse("[1, 2].filter(x, [3, 4].all(y, x < y))"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
}

TEST(FlatExprBuilderComprehensionsTest, MapComp) {
  FlatExprBuilder builder;
  builder.set_enable_comprehension_list_append(true);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr, parser::Parse("[1, 2].map(x, x * 2)"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
  EXPECT_THAT((*result.ListOrDie())[0],
              test::EqualsCelValue(CelValue::CreateInt64(2)));
  EXPECT_THAT((*result.ListOrDie())[1],
              test::EqualsCelValue(CelValue::CreateInt64(4)));
}

TEST(FlatExprBuilderComprehensionsTest, InvalidComprehensionWithRewrite) {
  CheckedExpr expr;
  // The rewrite step which occurs when an identifier gets a more qualified name
  // from the reference map has the potential to make invalid comprehensions
  // appear valid, by populating missing fields with default values.
  // var.<macro>(x, <missing>)
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
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
                       testing::AnyOf(HasSubstr("Invalid comprehension"),
                                      HasSubstr("Invalid empty expression"))));
}

TEST(FlatExprBuilderComprehensionsTest, ComprehensionWithConcatVulernability) {
  CheckedExpr expr;
  // The comprehension loop step performs an unsafe concatenation of the
  // accumulation variable with itself or one of its children.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              call_expr {
                function: "_?_:_"
                args { const_expr { bool_value: true } }
                args { ident_expr { name: "y" } }
                args {
                  call_expr {
                    function: "_+_"
                    args {
                      call_expr {
                        function: "dyn"
                        args { ident_expr { name: "y" } }
                      }
                    }
                    args {
                      call_expr {
                        function: "_[_]"
                        args { ident_expr { name: "y" } }
                        args { const_expr { int64_value: 0 } }
                      }
                    }
                  }
                }
              }
            }
          }
        })pb",
      &expr);

  FlatExprBuilder builder;
  builder.set_enable_comprehension_vulnerability_check(true);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST(FlatExprBuilderComprehensionsTest, ComprehensionWithListVulernability) {
  CheckedExpr expr;
  // The comprehension
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              list_expr {
                elements { ident_expr { name: "y" } }
                elements {
                  list_expr {
                    elements {
                      select_expr {
                        operand { ident_expr { name: "y" } }
                        field: "z"
                      }
                    }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  FlatExprBuilder builder;
  builder.set_enable_comprehension_vulnerability_check(true);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST(FlatExprBuilderComprehensionsTest, ComprehensionWithStructVulernability) {
  CheckedExpr expr;
  // The comprehension loop step builds a deeply nested struct which expands
  // exponentially.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              struct_expr {
                entries {
                  map_key { const_expr { string_value: "key" } }
                  value { ident_expr { name: "y" } }
                }
                entries {
                  map_key { const_expr { string_value: "present" } }
                  value {
                    select_expr {
                      test_only: true
                      operand { ident_expr { name: "y" } }
                      field: "z"
                    }
                  }
                }
                entries {
                  map_key { const_expr { string_value: "key_subset" } }
                  value {
                    select_expr {
                      operand { ident_expr { name: "y" } }
                      field: "z"
                    }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  FlatExprBuilder builder;
  builder.set_enable_comprehension_vulnerability_check(true);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST(FlatExprBuilderComprehensionsTest,
     ComprehensionWithNestedComprehensionResultVulernability) {
  CheckedExpr expr;
  // The nested comprehension performs an unsafe concatenation on the parent
  // accumulator variable within its 'result' expression.
  //
  // The inner-most comprehension shadows its parent, but still refers to its
  // oldest ancestor. It, however, does not do anything unsafe.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr { comprehension_expr {
          iter_var: "x"
          iter_range { ident_expr { name: "var" } }
          accu_var: "y"
          accu_init { list_expr {} }
          result { ident_expr { name: "y" } }
          loop_condition { const_expr { bool_value: true } }
          loop_step {
            comprehension_expr {
              iter_var: "x"
              iter_range { ident_expr { name: "y" } }
              accu_var: "z"
              accu_init { list_expr {} }
              result {
                call_expr {
                  function: "_+_"
                  args { ident_expr { name: "y" } }
                  args { ident_expr { name: "y" } }
                }
              }
              loop_condition { const_expr { bool_value: true } }
              loop_step {
                comprehension_expr {
                  iter_var: "x"
                  iter_range { ident_expr { name: "y" } }
                  accu_var: "z"
                  accu_init { list_expr {} }
                  result {
                    call_expr {
                      function: "dyn"
                      args { ident_expr { name: "y" } }
                    }
                  }
                  loop_condition { const_expr { bool_value: true } }
                  loop_step {
                    call_expr {
                      function: "dyn"
                      args { ident_expr { name: "y" } }
                    }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  FlatExprBuilder builder;
  builder.set_enable_comprehension_vulnerability_check(true);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST(FlatExprBuilderComprehensionsTest,
     ComprehensionWithNestedComprehensionLoopStepVulernability) {
  CheckedExpr expr;
  // The nested comprehension performs an unsafe concatenation on the parent
  // accumulator variable within its 'loop_step'.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              comprehension_expr {
                iter_var: "x"
                iter_range { ident_expr { name: "y" } }
                accu_var: "z"
                accu_init { list_expr {} }
                result { ident_expr { name: "z" } }
                loop_condition { const_expr { bool_value: true } }
                loop_step {
                  call_expr {
                    function: "_+_"
                    args { ident_expr { name: "y" } }
                    args { ident_expr { name: "y" } }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  FlatExprBuilder builder;
  builder.set_enable_comprehension_vulnerability_check(true);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST(FlatExprBuilderComprehensionsTest,
     ComprehensionWithNestedComprehensionLoopStepVulernabilityResult) {
  CheckedExpr expr;
  // The nested comprehension performs an unsafe concatenation on the parent
  // accumulator.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "outer_iter"
            iter_range { ident_expr { name: "input_list" } }
            accu_var: "outer_accu"
            accu_init { ident_expr { name: "input_list" } }
            loop_condition {
              id: 3
              const_expr { bool_value: true }
            }
            loop_step {
              comprehension_expr {
                # the iter_var shadows the outer accumulator on the loop step
                # but not the result step.
                iter_var: "outer_accu"
                iter_range { list_expr {} }
                accu_var: "inner_accu"
                accu_init { list_expr {} }
                loop_condition { const_expr { bool_value: true } }
                loop_step { list_expr {} }
                result {
                  call_expr {
                    function: "_+_"
                    args { ident_expr { name: "outer_accu" } }
                    args { ident_expr { name: "outer_accu" } }
                  }
                }
              }
            }
            result { list_expr {} }
          }
        }
      )pb",
      &expr);
  FlatExprBuilder builder;
  builder.set_enable_comprehension_vulnerability_check(true);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST(FlatExprBuilderComprehensionsTest,
     ComprehensionWithNestedComprehensionLoopStepIterRangeVulnerability) {
  CheckedExpr expr;
  // The nested comprehension unsafely modifies the parent accumulator
  // (outer_accu) being used as a iterable range
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "input_list" } }
            accu_var: "outer_accu"
            accu_init { ident_expr { name: "input_list" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              comprehension_expr {
                iter_var: "y"
                iter_range { ident_expr { name: "outer_accu" } }
                accu_var: "inner_accu"
                accu_init { ident_expr { name: "outer_accu" } }
                loop_condition { const_expr { bool_value: true } }
                loop_step {
                  call_expr {
                    function: "_+_"
                    args { ident_expr { name: "inner_accu" } }
                    args { const_expr { string_value: "12345" } }
                  }
                }
                result { ident_expr { name: "inner_accu" } }
              }
            }
            result { ident_expr { name: "outer_accu" } }
          }
        }
      )pb",
      &expr);
  FlatExprBuilder builder;
  builder.set_enable_comprehension_vulnerability_check(true);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

}  // namespace

}  // namespace google::api::expr::runtime
