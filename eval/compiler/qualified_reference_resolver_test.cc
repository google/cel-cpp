#include "eval/compiler/qualified_reference_resolver.h"

#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "testutil/util.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::Reference;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Optional;
using testing::UnorderedElementsAre;
using testutil::EqualsProto;

// foo.bar.var1 && bar.foo.var2
constexpr char kExpr[] = R"(
  id: 1
  call_expr {
    function: "_&&_"
    args {
      id: 2
      select_expr {
        field: "var1"
        operand {
          id: 3
          select_expr {
            field: "bar"
            operand {
              id: 4
              ident_expr { name: "foo" }
            }
          }
        }
      }
    }
    args {
      id: 5
      select_expr {
        field: "var2"
        operand {
          id: 6
          select_expr {
            field: "foo"
            operand {
              id: 7
              ident_expr { name: "bar" }
            }
          }
        }
      }
    }
  }
)";

MATCHER_P(StatusCodeIs, x, "") {
  const absl::Status& status = arg;
  return status.code() == x;
}

Expr ParseTestProto(const std::string& pb) {
  Expr expr;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(pb, &expr));
  return expr;
}

TEST(ResolveReferences, Basic) {
  Expr expr = ParseTestProto(kExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  reference_map[2].set_name("foo.bar.var1");
  reference_map[5].set_name("bar.foo.var2");
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result);
  EXPECT_THAT(result.value(), Optional(EqualsProto(R"(
                id: 1
                call_expr {
                  function: "_&&_"
                  args {
                    id: 2
                    ident_expr { name: "foo.bar.var1" }
                  }
                  args {
                    id: 5
                    ident_expr { name: "bar.foo.var2" }
                  }
                })")));
}

TEST(ResolveReferences, ReturnsNulloptIfNoChanges) {
  Expr expr = ParseTestProto(kExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
}

TEST(ResolveReferences, NamespacedIdent) {
  Expr expr = ParseTestProto(kExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;

  reference_map[2].set_name("foo.bar.var1");
  reference_map[7].set_name("namespace_x.bar");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Optional(EqualsProto(R"(
                id: 1
                call_expr {
                  function: "_&&_"
                  args {
                    id: 2
                    ident_expr { name: "foo.bar.var1" }
                  }
                  args {
                    id: 5
                    select_expr {
                      field: "var2"
                      operand {
                        id: 6
                        select_expr {
                          field: "foo"
                          operand {
                            id: 7
                            ident_expr { name: "namespace_x.bar" }
                          }
                        }
                      }
                    }
                  }
                })")));
}

TEST(ResolveReferences, WarningOnPresenceTest) {
  Expr expr = ParseTestProto(R"(
    id: 1
    select_expr {
      field: "var1"
      test_only: true
      operand {
        id: 2
        select_expr {
          field: "bar"
          operand {
            id: 3
            ident_expr { name: "foo" }
          }
        }
      }
    })");
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  reference_map[1].set_name("foo.bar.var1");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(
      warnings.warnings(),
      testing::ElementsAre(Eq(absl::Status(
          absl::StatusCode::kInvalidArgument,
          "Reference map points to a presence test -- has(container.attr)"))));
}

// foo.bar.var1 == bar.foo.Enum.ENUM_VAL1
constexpr char kEnumExpr[] = R"(
  id: 1
  call_expr {
    function: "_==_"
    args {
      id: 2
      select_expr {
        field: "var1"
        operand {
          id: 3
          select_expr {
            field: "bar"
            operand {
              id: 4
              ident_expr { name: "foo" }
            }
          }
        }
      }
    }
    args {
      id: 5
      ident_expr { name: "bar.foo.Enum.ENUM_VAL1" }
    }
  }
)";
TEST(ResolveReferences, EnumConstReferenceUsed) {
  Expr expr = ParseTestProto(kEnumExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));
  reference_map[2].set_name("foo.bar.var1");
  reference_map[5].set_name("bar.foo.Enum.ENUM_VAL1");
  reference_map[5].mutable_value()->set_int64_value(9);
  BuilderWarnings warnings;
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result);
  EXPECT_THAT(result.value(), Optional(EqualsProto(R"(
                id: 1
                call_expr {
                  function: "_==_"
                  args {
                    id: 2
                    ident_expr { name: "foo.bar.var1" }
                  }
                  args {
                    id: 5
                    const_expr { int64_value: 9 }
                  }
                })")));
}

TEST(ResolveReferences, ConstReferenceSkipped) {
  Expr expr = ParseTestProto(kExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));
  reference_map[2].set_name("foo.bar.var1");
  reference_map[2].mutable_value()->set_bool_value(true);
  reference_map[5].set_name("bar.foo.var2");
  BuilderWarnings warnings;
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result);
  EXPECT_THAT(result.value(), Optional(EqualsProto(R"(
                id: 1
                call_expr {
                  function: "_&&_"
                  args {
                    id: 2
                    select_expr {
                      field: "var1"
                      operand {
                        id: 3
                        select_expr {
                          field: "bar"
                          operand {
                            id: 4
                            ident_expr { name: "foo" }
                          }
                        }
                      }
                    }
                  }
                  args {
                    id: 5
                    ident_expr { name: "bar.foo.var2" }
                  }
                })")));
}

constexpr char kExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  args {
    id: 2
    const_expr {
      bool_value: true
    }
  }
  args {
    id: 3
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceBasic) {
  Expr expr = ParseTestProto(kExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.RegisterLazyFunction(
      CelFunctionDescriptor("boolean_and", false,
                            {
                                CelValue::Type::kBool,
                                CelValue::Type::kBool,
                            })));
  BuilderWarnings warnings;
  reference_map[1].add_overload_id("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
}

TEST(ResolveReferences, FunctionReferenceMissingOverloadDetected) {
  Expr expr = ParseTestProto(kExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  CelFunctionRegistry registry;
  BuilderWarnings warnings;
  reference_map[1].add_overload_id("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(warnings.warnings(),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

TEST(ResolveReferences, SpecialBuiltinsNotWarned) {
  Expr expr = ParseTestProto(R"(
    id: 1
    call_expr {
      function: "*"
      args {
        id: 2
        const_expr { bool_value: true }
      }
      args {
        id: 3
        const_expr { bool_value: false }
      }
    })");

  std::vector<const char*> special_builtins{builtin::kAnd, builtin::kOr,
                                            builtin::kTernary, builtin::kIndex};
  for (const char* builtin_fn : special_builtins) {
    google::protobuf::Map<int64_t, Reference> reference_map;
    // Builtins aren't in the function registry.
    CelFunctionRegistry registry;
    BuilderWarnings warnings;
    reference_map[1].add_overload_id(absl::StrCat("builtin.", builtin_fn));
    expr.mutable_call_expr()->set_function(builtin_fn);
    auto result =
        ResolveReferences(expr, reference_map, registry, "", &warnings);
    ASSERT_OK(result.status());
    EXPECT_THAT(result.value(), Eq(absl::nullopt));
    EXPECT_THAT(warnings.warnings(), IsEmpty());
  }
}

TEST(ResolveReferences,
     FunctionReferenceMissingOverloadDetectedAndMissingReference) {
  Expr expr = ParseTestProto(kExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  CelFunctionRegistry registry;
  BuilderWarnings warnings;
  reference_map[1].set_name("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(
      warnings.warnings(),
      UnorderedElementsAre(
          Eq(absl::InvalidArgumentError(
              "No overload found in reference resolve step for boolean_and")),
          Eq(absl::InvalidArgumentError(
              "Reference map doesn't provide overloads for boolean_and"))));
}

TEST(ResolveReferences, FunctionReferenceToWrongExprKind) {
  Expr expr = ParseTestProto(kExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  reference_map[2].add_overload_id("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(warnings.warnings(),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

constexpr char kReceiverCallExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  target {
    id: 2
    ident_expr {
      name: "ext"
    }
  }
  args {
    id: 3
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceWithTargetNoChange) {
  Expr expr = ParseTestProto(kReceiverCallExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.RegisterLazyFunction(CelFunctionDescriptor(
      "boolean_and", true, {CelValue::Type::kBool, CelValue::Type::kBool})));
  reference_map[1].add_overload_id("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

TEST(ResolveReferences,
     FunctionReferenceWithTargetNoChangeMissingOverloadDetected) {
  Expr expr = ParseTestProto(kReceiverCallExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  reference_map[1].add_overload_id("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(warnings.warnings(),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

TEST(ResolveReferences, FunctionReferenceWithTargetToNamespacedFunction) {
  Expr expr = ParseTestProto(kReceiverCallExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.RegisterLazyFunction(CelFunctionDescriptor(
      "ext.boolean_and", false, {CelValue::Type::kBool})));
  reference_map[1].add_overload_id("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Optional(EqualsProto(R"(
                id: 1
                call_expr {
                  function: "ext.boolean_and"
                  args {
                    id: 3
                    const_expr { bool_value: false }
                  }
                }
              )")));
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

TEST(ResolveReferences,
     FunctionReferenceWithTargetToNamespacedFunctionInContainer) {
  Expr expr = ParseTestProto(kReceiverCallExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  reference_map[1].add_overload_id("udf_boolean_and");
  ASSERT_OK(registry.RegisterLazyFunction(CelFunctionDescriptor(
      "com.google.ext.boolean_and", false, {CelValue::Type::kBool})));
  auto result =
      ResolveReferences(expr, reference_map, registry, "com.google", &warnings);
  ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), Optional(EqualsProto(R"(
                id: 1
                call_expr {
                  function: "com.google.ext.boolean_and"
                  args {
                    id: 3
                    const_expr { bool_value: false }
                  }
                }
              )")));
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

// has(ext.option).boolean_and(false)
constexpr char kReceiverCallHasExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  target {
    id: 2
    select_expr {
      test_only: true
      field: "option"
      operand {
        id: 3
        ident_expr {
          name: "ext"
        }
      }
    }
  }
  args {
    id: 4
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceWithHasTargetNoChange) {
  Expr expr = ParseTestProto(kReceiverCallHasExtensionAndExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  CelFunctionRegistry registry;
  ASSERT_OK(registry.RegisterLazyFunction(CelFunctionDescriptor(
      "boolean_and", true, {CelValue::Type::kBool, CelValue::Type::kBool})));
  ASSERT_OK(registry.RegisterLazyFunction(CelFunctionDescriptor(
      "ext.option.boolean_and", true, {CelValue::Type::kBool})));
  reference_map[1].add_overload_id("udf_boolean_and");
  auto result = ResolveReferences(expr, reference_map, registry, "", &warnings);
  ASSERT_OK(result.status());
  // The target is unchanged because it is a test_only select.
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(warnings.warnings(), IsEmpty());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
