package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "cel_value_internal",
    hdrs = [
        "cel_value_internal.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "cel_value",
    srcs = [
        "cel_value.cc",
    ],
    hdrs = [
        "cel_value.h",
    ],
    deps = [
        ":cel_value_internal",
        "//base:status",
        "//internal:proto_util",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_value_producer",
    hdrs = [
        "cel_value_producer.h",
    ],
    deps = [
        ":cel_value",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "activation",
    srcs = [
        "activation.cc",
    ],
    hdrs = [
        "activation.h",
    ],
    deps = [
        ":cel_value",
        ":cel_value_producer",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "activation_bind_helper",
    srcs = [
        "activation_bind_helper.cc",
    ],
    hdrs = [
        "activation_bind_helper.h",
    ],
    deps = [
        ":activation",
        "//eval/eval:field_access",
        "//eval/eval:field_backed_list_impl",
        "//eval/eval:field_backed_map_impl",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "cel_function",
    srcs = [
        "cel_function.cc",
    ],
    hdrs = [
        "cel_function.h",
    ],
    deps = [
        ":cel_options",
        ":cel_value",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "cel_function_adapter",
    srcs = [
        "cel_function_adapter.cc",
    ],
    hdrs = [
        "cel_function_adapter.h",
    ],
    deps = [
        ":cel_function",
        "//base:status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_builtins",
    hdrs = [
        "cel_builtins.h",
    ],
    deps = [
    ],
)

cc_library(
    name = "builtin_func_registrar",
    srcs = [
        "builtin_func_registrar.cc",
    ],
    hdrs = [
        "builtin_func_registrar.h",
    ],
    deps = [
        ":cel_builtins",
        ":cel_function",
        ":cel_function_adapter",
        ":cel_options",
        "//base:status",
        "//eval/eval:container_backed_list_impl",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "extension_func_registrar",
    srcs = [
        "extension_func_registrar.cc",
    ],
    hdrs = [
        "extension_func_registrar.h",
    ],
    deps = [
        ":cel_function",
        ":cel_function_adapter",
        ":cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_expression",
    hdrs = [
        "cel_expression.h",
    ],
    deps = [
        ":activation",
        ":cel_function",
        ":cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "source_position",
    srcs = ["source_position.cc"],
    hdrs = ["source_position.h"],
    deps = [
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_library(
    name = "ast_visitor",
    hdrs = [
        "ast_visitor.h",
    ],
    deps = [
        ":source_position",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_visitor_base",
    hdrs = [
        "ast_visitor_base.h",
    ],
    deps = [
        ":ast_visitor",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_traverse",
    srcs = [
        "ast_traverse.cc",
    ],
    hdrs = [
        "ast_traverse.h",
    ],
    deps = [
        ":ast_visitor",
        ":source_position",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "cel_options",
    hdrs = [
        "cel_options.h",
    ],
    deps = [
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_expr_builder_factory",
    srcs = [
        "cel_expr_builder_factory.cc",
    ],
    hdrs = [
        "cel_expr_builder_factory.h",
    ],
    deps = [
        ":cel_expression",
        ":cel_options",
        "//eval/compiler:flat_expr_builder",
    ],
)

cc_library(
    name = "value_export_util",
    srcs = [
        "value_export_util.cc",
    ],
    hdrs = [
        "value_export_util.h",
    ],
    deps = [
        ":cel_value",
        "//base:status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_value_test",
    size = "small",
    srcs = [
        "cel_value_test.cc",
    ],
    deps = [
        ":cel_value",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "activation_test",
    size = "small",
    srcs = [
        "activation_test.cc",
    ],
    deps = [
        ":activation",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ast_traverse_test",
    srcs = [
        "ast_traverse_test.cc",
    ],
    deps = [
        ":ast_traverse",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "activation_bind_helper_test",
    size = "small",
    srcs = [
        "activation_bind_helper_test.cc",
    ],
    deps = [
        ":activation",
        ":activation_bind_helper",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_function_adapter_test",
    size = "small",
    srcs = [
        "cel_function_adapter_test.cc",
    ],
    deps = [
        ":cel_function",
        ":cel_function_adapter",
        ":cel_value",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "builtin_func_test",
    size = "small",
    srcs = [
        "builtin_func_test.cc",
    ],
    deps = [
        ":activation",
        ":builtin_func_registrar",
        ":cel_builtins",
        ":cel_expr_builder_factory",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "extension_func_test",
    size = "small",
    srcs = [
        "extension_func_test.cc",
    ],
    deps = [
        ":builtin_func_registrar",
        ":cel_value",
        ":extension_func_registrar",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "source_position_test",
    size = "small",
    srcs = [
        "source_position_test.cc",
    ],
    deps = [
        ":source_position",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "value_export_util_test",
    size = "small",
    srcs = [
        "value_export_util_test.cc",
    ],
    deps = [
        ":cel_value",
        ":value_export_util",
        "//base:status",
        "//eval/eval:container_backed_list_impl",
        "//eval/eval:container_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
