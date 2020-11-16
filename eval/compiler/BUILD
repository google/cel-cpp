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
    deps = [
        ":constant_folding",
        ":qualified_reference_resolver",
        "//base:status_macros",
        "//eval/eval:comprehension_step",
        "//eval/eval:const_value_step",
        "//eval/eval:container_access_step",
        "//eval/eval:create_list_step",
        "//eval/eval:create_struct_step",
        "//eval/eval:evaluator_core",
        "//eval/eval:expression_build_warning",
        "//eval/eval:function_step",
        "//eval/eval:ident_step",
        "//eval/eval:jump_step",
        "//eval/eval:logic_step",
        "//eval/eval:select_step",
        "//eval/eval:ternary_step",
        "//eval/public:ast_traverse",
        "//eval/public:ast_visitor",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "//eval/public:cel_function_registry",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
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
        "//base:status_macros",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_attribute",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "//eval/public:cel_function_adapter",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "flat_expr_builder_comprehensions_test",
    srcs = [
        "flat_expr_builder_comprehensions_test.cc",
    ],
    deps = [
        ":flat_expr_builder",
        "//base:status_macros",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_attribute",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/status",
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
        "//eval/public:cel_builtins",
        "//eval/public:cel_function",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_list_impl",
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
        "//base:status_macros",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_function_registry",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "qualified_reference_resolver",
    srcs = [
        "qualified_reference_resolver.cc",
    ],
    hdrs = [
        "qualified_reference_resolver.h",
    ],
    deps = [
        "//base:status_macros",
        "//eval/eval:const_value_step",
        "//eval/eval:expression_build_warning",
        "//eval/public:cel_builtins",
        "//eval/public:cel_function_registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "qualified_reference_resolver_test",
    srcs = [
        "qualified_reference_resolver_test.cc",
    ],
    deps = [
        ":qualified_reference_resolver",
        "//base:status_macros",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_builtins",
        "//eval/public:cel_function",
        "//eval/public:cel_function_registry",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "flat_expr_builder_short_circuiting_conformance_test",
    srcs = [
        "flat_expr_builder_short_circuiting_conformance_test.cc",
    ],
    deps = [
        ":flat_expr_builder",
        "//base:status_macros",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
