#include <memory>

#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_set.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using google::api::expr::v1alpha1::Expr;
using ::google::protobuf::Arena;
using testing::ElementsAre;

// var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
constexpr char kExprTextproto[] = R"pb(
  id: 13
  call_expr {
    function: "_||_"
    args {
      id: 6
      call_expr {
        function: "_&&_"
        args {
          id: 2
          call_expr {
            function: "_>_"
            args {
              id: 1
              ident_expr { name: "var1" }
            }
            args {
              id: 3
              const_expr { int64_value: 3 }
            }
          }
        }
        args {
          id: 4
          call_expr {
            function: "F1"
            args {
              id: 5
              const_expr { string_value: "arg1" }
            }
          }
        }
      }
    }
    args {
      id: 12
      call_expr {
        function: "_&&_"
        args {
          id: 8
          call_expr {
            function: "_>_"
            args {
              id: 7
              ident_expr { name: "var2" }
            }
            args {
              id: 9
              const_expr { int64_value: 3 }
            }
          }
        }
        args {
          id: 10
          call_expr {
            function: "F2"
            args {
              id: 11
              const_expr { string_value: "arg2" }
            }
          }
        }
      }
    }
  })pb";

enum class FunctionResponse { kUnknown, kTrue, kFalse };

CelFunctionDescriptor CreateDescriptor(
    absl::string_view name, CelValue::Type type = CelValue::Type::kString) {
  return CelFunctionDescriptor(std::string(name), false, {type});
}

class FunctionImpl : public CelFunction {
 public:
  FunctionImpl(absl::string_view name, FunctionResponse response,
               CelValue::Type type = CelValue::Type::kString)
      : CelFunction(CreateDescriptor(name, type)), response_(response) {}

  absl::Status Evaluate(absl::Span<const CelValue> arguments, CelValue* result,
                        Arena* arena) const override {
    switch (response_) {
      case FunctionResponse::kUnknown:
        *result = CreateUnknownFunctionResultError(arena, "help message");
        break;
      case FunctionResponse::kTrue:
        *result = CelValue::CreateBool(true);
        break;
      case FunctionResponse::kFalse:
        *result = CelValue::CreateBool(false);
        break;
    }
    return absl::OkStatus();
  }

 private:
  FunctionResponse response_;
};

// Text fixture for unknowns. Holds on to state needed for execution to work
// correctly.
class UnknownsTest : public testing::Test {
 public:
  void PrepareBuilder(UnknownProcessingOptions opts) {
    InterpreterOptions options;
    options.unknown_processing = opts;
    builder_ = CreateCelExpressionBuilder(options);
    ASSERT_OK(RegisterBuiltinFunctions(builder_->GetRegistry()));
    ASSERT_OK(
        builder_->GetRegistry()->RegisterLazyFunction(CreateDescriptor("F1")));
    ASSERT_OK(
        builder_->GetRegistry()->RegisterLazyFunction(CreateDescriptor("F2")));
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kExprTextproto, &expr_))
        << "error parsing expr";
  }

 protected:
  Arena arena_;
  Activation activation_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  google::api::expr::v1alpha1::Expr expr_;
};

MATCHER_P2(FunctionCallIs, fn_name, fn_arg, "") {
  const UnknownFunctionResult* result = arg;
  return result->arguments().size() == 1 && result->arguments()[0].IsString() &&
         result->arguments()[0].StringOrDie().value() == fn_arg &&
         result->descriptor().name() == fn_name;
}

MATCHER_P(AttributeIs, attr, "") {
  const CelAttribute* result = arg;
  return result->variable().ident_expr().name() == attr;
}

TEST_F(UnknownsTest, NoUnknowns) {
  PrepareBuilder(UnknownProcessingOptions::kDisabled);
  // activation_.set_unknown_attribute_patterns({CelAttributePattern("var1",
  // {})});
  activation_.InsertValue("var1", CelValue::CreateInt64(3));
  activation_.InsertValue("var2", CelValue::CreateInt64(5));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kFalse)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kTrue)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsBool());
  EXPECT_TRUE(response.BoolOrDie());
}

TEST_F(UnknownsTest, UnknownAttributes) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeOnly);
  activation_.set_unknown_attribute_patterns({CelAttributePattern("var1", {})});
  activation_.InsertValue("var2", CelValue::CreateInt64(3));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kTrue)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kFalse)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsUnknownSet());
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_attributes().attributes(),
              ElementsAre(AttributeIs("var1")));
}

TEST_F(UnknownsTest, UnknownAttributesPruning) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeOnly);
  activation_.set_unknown_attribute_patterns({CelAttributePattern("var1", {})});
  activation_.InsertValue("var2", CelValue::CreateInt64(5));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kTrue)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kTrue)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsBool());
  EXPECT_TRUE(response.BoolOrDie());
}

TEST_F(UnknownsTest, UnknownFunctionsWithoutOptionError) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeOnly);
  activation_.InsertValue("var1", CelValue::CreateInt64(5));
  activation_.InsertValue("var2", CelValue::CreateInt64(3));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kUnknown)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kFalse)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsError());
  EXPECT_EQ(response.ErrorOrDie()->code(), absl::StatusCode::kUnavailable);
}

TEST_F(UnknownsTest, UnknownFunctions) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);
  activation_.InsertValue("var1", CelValue::CreateInt64(5));
  activation_.InsertValue("var2", CelValue::CreateInt64(5));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kUnknown)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kFalse)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsUnknownSet()) << response.ErrorOrDie()->ToString();
  EXPECT_THAT(response.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              ElementsAre(FunctionCallIs("F1", "arg1")));
}

TEST_F(UnknownsTest, UnknownsMerge) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);
  activation_.InsertValue("var1", CelValue::CreateInt64(5));
  activation_.set_unknown_attribute_patterns({CelAttributePattern("var2", {})});

  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kUnknown)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kTrue)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsUnknownSet()) << response.ErrorOrDie()->ToString();
  EXPECT_THAT(response.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              ElementsAre(FunctionCallIs("F1", "arg1")));
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_attributes().attributes(),
              ElementsAre(AttributeIs("var2")));
}

constexpr char kListCompExistsExpr[] = R"pb(
  id: 25
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
        elements {
          id: 4
          const_expr { int64_value: 3 }
        }
        elements {
          id: 5
          const_expr { int64_value: 4 }
        }
        elements {
          id: 6
          const_expr { int64_value: 5 }
        }
        elements {
          id: 7
          const_expr { int64_value: 6 }
        }
        elements {
          id: 8
          const_expr { int64_value: 7 }
        }
        elements {
          id: 9
          const_expr { int64_value: 8 }
        }
        elements {
          id: 10
          const_expr { int64_value: 9 }
        }
        elements {
          id: 11
          const_expr { int64_value: 10 }
        }
      }
    }
    accu_var: "__result__"
    accu_init {
      id: 18
      const_expr { bool_value: false }
    }
    loop_condition {
      id: 21
      call_expr {
        function: "@not_strictly_false"
        args {
          id: 20
          call_expr {
            function: "!_"
            args {
              id: 19
              ident_expr { name: "__result__" }
            }
          }
        }
      }
    }
    loop_step {
      id: 23
      call_expr {
        function: "_||_"
        args {
          id: 22
          ident_expr { name: "__result__" }
        }
        args {
          id: 16
          call_expr {
            function: "_>_"
            args {
              id: 14
              call_expr {
                function: "Fn"
                args {
                  id: 15
                  ident_expr { name: "x" }
                }
              }
            }
            args {
              id: 17
              const_expr { int64_value: 2 }
            }
          }
        }
      }
    }
    result {
      id: 24
      ident_expr { name: "__result__" }
    }
  })pb";

// Text fixture for comprehension tests. Holds on to state needed for execution
// to work correctly.
class UnknownsCompTest : public testing::Test {
 public:
  void PrepareBuilder(UnknownProcessingOptions opts) {
    InterpreterOptions options;
    options.unknown_processing = opts;
    builder_ = CreateCelExpressionBuilder(options);
    ASSERT_OK(RegisterBuiltinFunctions(builder_->GetRegistry()));
    ASSERT_OK(builder_->GetRegistry()->RegisterLazyFunction(
        CreateDescriptor("Fn", CelValue::Type::kInt64)));
    ASSERT_TRUE(
        google::protobuf::TextFormat::ParseFromString(kListCompExistsExpr, &expr_))
        << "error parsing expr";
  }

 protected:
  Arena arena_;
  Activation activation_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  Expr expr_;
};

TEST_F(UnknownsCompTest, UnknownsMerge) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);

  ASSERT_OK(activation_.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kUnknown, CelValue::Type::kInt64)));

  // [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].exists(x, Fn(x) > 5)
  auto build_status = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(build_status);

  auto eval_status = build_status.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(eval_status);
  CelValue response = eval_status.value();

  ASSERT_TRUE(response.IsUnknownSet()) << response.ErrorOrDie()->ToString();
  EXPECT_THAT(response.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              testing::SizeIs(10));
}

constexpr char kListCompCondExpr[] = R"pb(
  id: 25
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
        elements {
          id: 11
          const_expr { int64_value: 3 }
        }
      }
    }
    accu_var: "__result__"
    accu_init {
      id: 18
      const_expr { int64_value: 0 }
    }
    loop_condition {
      id: 21
      call_expr {
        function: "_<=_"
        args {
          id: 20
          ident_expr { name: "__result__" }
        }
        args {
          id: 19
          const_expr { int64_value: 1 }
        }
      }
    }
    loop_step {
      id: 23
      call_expr {
        function: "_?_:_"
        args {
          id: 22
          call_expr: {
            function: "Fn"
            args {
              id: 4
              ident_expr { name: "x" }
            }
          }
        }
        args {
          id: 14
          call_expr {
            function: "_+_"
            args {
              id: 15
              ident_expr { name: "__result__" }
            }
            args {
              id: 17
              const_expr { int64_value: 1 }
            }
          }
        }
        args {
          id: 16
          ident_expr { name: "__result__" }
        }
      }
    }
    result {
      id: 24
      call_expr {
        function: "_==_"
        args {
          id: 27
          ident_expr { name: "__result__" }
        }
        args {
          id: 26
          const_expr { int64_value: 1 }
        }
      }
    }
  })pb";

// Text fixture for comprehension tests affecting the condition step.
// Holds on to state needed for execution to work correctly.
class UnknownsCompCondTest : public testing::Test {
 public:
  void PrepareBuilder(UnknownProcessingOptions opts) {
    InterpreterOptions options;
    options.unknown_processing = opts;
    builder_ = CreateCelExpressionBuilder(options);
    ASSERT_OK(RegisterBuiltinFunctions(builder_->GetRegistry()));
    ASSERT_OK(builder_->GetRegistry()->RegisterLazyFunction(
        CreateDescriptor("Fn", CelValue::Type::kInt64)));
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kListCompCondExpr, &expr_))
        << "error parsing expr";
  }

 protected:
  Arena arena_;
  Activation activation_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  Expr expr_;
};

TEST_F(UnknownsCompCondTest, UnknownConditionReturned) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);

  ASSERT_OK(activation_.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kUnknown, CelValue::Type::kInt64)));

  // [1, 2, 3].exists_one(x, Fn(x))
  auto build_status = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(build_status);

  auto eval_status = build_status.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(eval_status);
  CelValue response = eval_status.value();

  ASSERT_TRUE(response.IsUnknownSet()) << response.ErrorOrDie()->ToString();
  // The comprehension ends on the first non-bool condition, so we only get one
  // call captured in the UnknownSet.
  EXPECT_THAT(response.UnknownSetOrDie()
                  ->unknown_function_results()
                  .unknown_function_results(),
              testing::SizeIs(1));
}

TEST_F(UnknownsCompCondTest, ErrorConditionReturned) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);

  // No implementation for Fn(int64_t) provided in activation -- this turns into a
  // CelError.
  // [1, 2, 3].exists_one(x, Fn(x))
  auto build_status = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(build_status);

  auto eval_status = build_status.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(eval_status);
  CelValue response = eval_status.value();

  ASSERT_TRUE(response.IsError()) << CelValue::TypeName(response.type());
  EXPECT_TRUE(CheckNoMatchingOverloadError(response));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
