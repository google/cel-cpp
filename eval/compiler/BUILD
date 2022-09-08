# This package contains code
# that compiles Expr object into evaluatable CelExpression
package(default_visibility = ["//visibility:public"])

licenses(["notice"])

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
        ":resolver",
        "//base:ast",
        "//base:ast_utility",
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
        "//eval/eval:regex_match_step",
        "//eval/eval:select_step",
        "//eval/eval:shadowable_value_step",
        "//eval/eval:ternary_step",
        "//eval/public:ast_traverse_native",
        "//eval/public:ast_visitor_native",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "//eval/public:cel_function_registry",
        "//eval/public:source_position",
        "//eval/public:source_position_native",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "flat_expr_builder_test",
    srcs = [
        "flat_expr_builder_test.cc",
    ],
    data = [
        "//eval/testutil:simple_test_message_proto",
    ],
    deps = [
        ":flat_expr_builder",
        "//eval/eval:expression_build_warning",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_attribute",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_function_adapter",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_descriptor_pool_builder",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:protobuf_descriptor_type_provider",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
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
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_attribute",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
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
        "//base:ast",
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
        "//base:ast_utility",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_value",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
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
        ":resolver",
        "//base:ast",
        "//eval/eval:const_value_step",
        "//eval/eval:expression_build_warning",
        "//eval/public:ast_rewrite_native",
        "//eval/public:cel_builtins",
        "//eval/public:cel_function_registry",
        "//eval/public:source_position_native",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "resolver",
    srcs = ["resolver.cc"],
    hdrs = ["resolver.h"],
    deps = [
        "//eval/public:cel_builtins",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_type_registry",
        "//eval/public:cel_value",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
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
        "//base:ast_utility",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_builtins",
        "//eval/public:cel_function",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_type_registry",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
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
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_builtins",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "resolver_test",
    size = "small",
    srcs = ["resolver_test.cc"],
    deps = [
        ":resolver",
        "//eval/public:cel_function",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_type_registry",
        "//eval/public:cel_value",
        "//eval/public/structs:protobuf_descriptor_type_provider",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

package_group(
    name = "native_api_users",
    packages = [
        "//eval/compiler",
    ],
)
