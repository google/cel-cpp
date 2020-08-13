#include "eval/compiler/qualified_reference_resolver.h"

#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
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
using testing::Optional;
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
  auto result = ResolveReferences(expr, reference_map, &warnings);
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
  auto result = ResolveReferences(expr, reference_map, &warnings);
  ASSERT_OK(result);
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
}

TEST(ResolveReferences, NamespacedIdent) {
  Expr expr = ParseTestProto(kExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  reference_map[2].set_name("foo.bar.var1");
  reference_map[7].set_name("namespace_x.bar");
  auto result = ResolveReferences(expr, reference_map, &warnings);
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

TEST(ResolveReferences, WarningOnUnsupportedExprKind) {
  Expr expr = ParseTestProto(
      R"(
        id: 1
        comprehension_expr {
          accu_init {
            id: 2
            const_expr { int64_value: 1 }
          }
          accu_var: "__result__"
          iter_var: "x"
          iter_range {
            id: 3
            list_expr {
              elements {
                id: 4
                const_expr { int64_value: 1 }
              }
            }
          }
          result {
            id: 6
            ident_expr { name: "__result__" }
          }
          loop_condition {
            id: 5
            const_expr { bool_value: true }
          }
          loop_step {
            id: 7
            call_expr {
              function: "_+_"
              args {
                id: 8
                ident_expr { name: "x" }
              }
              args {
                id: 9
                ident_expr { name: "__result__" }
              }
            }
          }
        })");
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  reference_map[1].set_name("foo");
  auto result = ResolveReferences(expr, reference_map, &warnings);
  ASSERT_OK(result);
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(warnings.warnings(),
              ElementsAre(Eq(absl::Status(absl::StatusCode::kInvalidArgument,
                                          "Unsupported reference kind: 9"))));
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
  reference_map[1].set_name("foo.bar.var1");
  auto result = ResolveReferences(expr, reference_map, &warnings);
  ASSERT_OK(result);
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
  EXPECT_THAT(
      warnings.warnings(),
      testing::ElementsAre(Eq(absl::Status(
          absl::StatusCode::kInvalidArgument,
          "Reference map points to a presence test -- has(container.attr)"))));
}

TEST(ResolveReferences, ConstReferenceSkipped) {
  Expr expr = ParseTestProto(kExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  reference_map[2].set_name("foo.bar.var1");
  reference_map[2].mutable_value()->set_bool_value(true);
  reference_map[5].set_name("bar.foo.var2");
  BuilderWarnings warnings;
  auto result = ResolveReferences(expr, reference_map, &warnings);
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

TEST(ResolveReferences, FunctionReferenceSkipped) {
  Expr expr = ParseTestProto(kExpr);
  google::protobuf::Map<int64_t, Reference> reference_map;
  BuilderWarnings warnings;
  reference_map[1].set_name("@user_defined_boolean_and");
  reference_map[1].add_overload_id("@user_defined_boolean_and_overload1");
  auto result = ResolveReferences(expr, reference_map, &warnings);
  ASSERT_OK(result);
  EXPECT_THAT(result.value(), Eq(absl::nullopt));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
