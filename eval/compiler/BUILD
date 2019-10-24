# This package contains code
# that compiles Expr object into evaluatable CelExpression
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "flat_expr_builder",
    srcs = [
        "flat_expr_builder.cc",
    ],
    hdrs = [
        "flat_expr_builder.h",
    ],
    visibility = [
        "//eval:__subpackages__",
        "//google/api/expr/conformance/evaluators/cel:__pkg__",
    ],
    deps = [
        ":constant_folding",
        "//base:status",
        "//eval/eval:comprehension_step",
        "//eval/eval:const_value_step",
        "//eval/eval:container_access_step",
        "//eval/eval:create_list_step",
        "//eval/eval:create_struct_step",
        "//eval/eval:evaluator_core",
        "//eval/eval:function_step",
        "//eval/eval:ident_step",
        "//eval/eval:jump_step",
        "//eval/eval:logic_step",
        "//eval/eval:select_step",
        "//eval/public:ast_traverse",
        "//eval/public:ast_visitor",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "flat_expr_builder_test",
    srcs = [
        "flat_expr_builder_test.cc",
    ],
    deps = [
        ":flat_expr_builder",
        "//eval/public:builtin_func_registrar",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "constant_folding",
    srcs = [
        "constant_folding.cc",
    ],
    hdrs = [
        "constant_folding.h",
    ],
    deps = [
        "//eval/eval:const_value_step",
        "//eval/eval:container_backed_list_impl",
        "//eval/public:cel_builtins",
        "//eval/public:cel_function",
        "//eval/public:cel_value",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "constant_folding_test",
    srcs = [
        "constant_folding_test.cc",
    ],
    deps = [
        ":constant_folding",
        "//eval/public:builtin_func_registrar",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
