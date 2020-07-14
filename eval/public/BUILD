package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "cel_value_internal",
    hdrs = [
        "cel_value_internal.h",
    ],
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":cel_value_internal",
        "//base:statusor",
        "//internal:proto_util",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_attribute",
    srcs = [
    ],
    hdrs = [
        "cel_attribute.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_value",
        ":cel_value_internal",
        "//base:statusor",
        "//internal:proto_util",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "cel_value_producer",
    hdrs = [
        "cel_value_producer.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_value",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "unknown_attribute_set",
    srcs = [
    ],
    hdrs = [
        "unknown_attribute_set.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_attribute",
        "@com_google_absl//absl/container:flat_hash_set",
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
    copts = ["-std=c++17"],
    deps = [
        ":cel_attribute",
        ":cel_function",
        ":cel_value",
        ":cel_value_producer",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":cel_value",
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
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_function_registry",
        "//base:statusor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_function_provider",
    srcs = [
        "cel_function_provider.cc",
    ],
    hdrs = [
        "cel_function_provider.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":activation",
        ":cel_function",
    ],
)

cc_library(
    name = "cel_builtins",
    hdrs = [
        "cel_builtins.h",
    ],
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":cel_builtins",
        ":cel_function",
        ":cel_function_adapter",
        ":cel_function_registry",
        ":cel_options",
        "//base:status_macros",
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
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_function_adapter",
        ":cel_function_registry",
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
    copts = ["-std=c++17"],
    deps = [
        ":activation",
        ":cel_function",
        ":cel_function_registry",
        ":cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "source_position",
    srcs = ["source_position.cc"],
    hdrs = ["source_position.h"],
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":cel_value",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_function_registry",
    srcs = ["cel_function_registry.cc"],
    hdrs = ["cel_function_registry.h"],
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_function_provider",
        ":cel_options",
        ":cel_value",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "cel_value_test",
    size = "small",
    srcs = [
        "cel_value_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_value",
        ":unknown_attribute_set",
        ":unknown_set",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_attribute_test",
    size = "small",
    srcs = [
        "cel_attribute_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_attribute",
        ":cel_value",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "activation_test",
    size = "small",
    srcs = [
        "activation_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":activation",
        ":cel_function",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ast_traverse_test",
    srcs = [
        "ast_traverse_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":ast_traverse",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "activation_bind_helper_test",
    size = "small",
    srcs = [
        "activation_bind_helper_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":activation",
        ":activation_bind_helper",
        "//base:status_macros",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_function_provider_test",
    srcs = [
        "cel_function_provider_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_function_provider",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cel_function_registry_test",
    srcs = [
        "cel_function_registry_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_function_provider",
        ":cel_function_registry",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cel_function_adapter_test",
    size = "small",
    srcs = [
        "cel_function_adapter_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_function_adapter",
        ":cel_value",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "builtin_func_test",
    size = "small",
    srcs = [
        "builtin_func_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":activation",
        ":builtin_func_registrar",
        ":cel_builtins",
        ":cel_expr_builder_factory",
        ":cel_function_registry",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "extension_func_test",
    size = "small",
    srcs = [
        "extension_func_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":builtin_func_registrar",
        ":cel_function_registry",
        ":cel_value",
        ":extension_func_registrar",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "source_position_test",
    size = "small",
    srcs = [
        "source_position_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":source_position",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "unknown_attribute_set_test",
    size = "small",
    srcs = [
        "unknown_attribute_set_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_attribute",
        ":cel_value",
        ":unknown_attribute_set",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "value_export_util_test",
    size = "small",
    srcs = [
        "value_export_util_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_value",
        ":value_export_util",
        "//base:status_macros",
        "//eval/eval:container_backed_list_impl",
        "//eval/eval:container_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "unknown_function_result_set",
    srcs = ["unknown_function_result_set.cc"],
    hdrs = ["unknown_function_result_set.h"],
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_options",
        ":cel_value",
        ":set_util",
        "@com_google_absl//absl/container:btree",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "unknown_function_result_set_test",
    size = "small",
    srcs = [
        "unknown_function_result_set_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_function",
        ":cel_value",
        ":unknown_function_result_set",
        "//eval/eval:container_backed_list_impl",
        "//eval/eval:container_backed_map_impl",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "unknown_set",
    hdrs = ["unknown_set.h"],
    copts = ["-std=c++17"],
    deps = [
        ":unknown_attribute_set",
        ":unknown_function_result_set",
    ],
)

cc_test(
    name = "unknown_set_test",
    srcs = ["unknown_set_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":cel_attribute",
        ":unknown_attribute_set",
        ":unknown_function_result_set",
        ":unknown_set",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "set_util",
    srcs = ["set_util.cc"],
    hdrs = ["set_util.h"],
    copts = ["-std=c++17"],
    deps = ["//eval/public:cel_value"],
)

cc_test(
    name = "set_util_test",
    size = "small",
    srcs = [
        "set_util_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":cel_value",
        ":set_util",
        ":unknown_set",
        "//eval/eval:container_backed_list_impl",
        "//eval/eval:container_backed_map_impl",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)
