package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "cel_value_internal",
    hdrs = [
        "cel_value_internal.h",
    ],
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
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_attribute",
    srcs = [
        "cel_attribute.cc",
    ],
    hdrs = [
        "cel_attribute.h",
    ],
    deps = [
        ":cel_value",
        ":cel_value_internal",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
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
    name = "unknown_attribute_set",
    srcs = [
    ],
    hdrs = [
        "unknown_attribute_set.h",
    ],
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
    deps = [
        ":activation",
        "//eval/public/containers:field_access",
        "//eval/public/containers:field_backed_list_impl",
        "//eval/public/containers:field_backed_map_impl",
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
    deps = [
        ":cel_function",
        ":cel_function_registry",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
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
    deps = [
        ":activation",
        ":cel_function",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "cel_builtins",
    hdrs = [
        "cel_builtins.h",
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
        ":cel_function_registry",
        ":cel_options",
        "//eval/public/containers:container_backed_list_impl",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
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
    deps = [
        ":activation",
        ":cel_function",
        ":cel_function_registry",
        ":cel_value",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "source_position",
    srcs = ["source_position.cc"],
    hdrs = ["source_position.h"],
    deps = [
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_visitor",
    hdrs = [
        "ast_visitor.h",
    ],
    deps = [
        ":source_position",
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
        "//internal:proto_util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_function_registry",
    srcs = ["cel_function_registry.cc"],
    hdrs = ["cel_function_registry.h"],
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
    deps = [
        ":cel_value",
        ":unknown_attribute_set",
        ":unknown_set",
        "//base:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cel_attribute_test",
    size = "small",
    srcs = [
        "cel_attribute_test.cc",
    ],
    deps = [
        ":cel_attribute",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/strings",
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
        ":cel_attribute",
        ":cel_function",
        "//base:status_macros",
        "//eval/eval:attribute_trail",
        "//eval/eval:ident_step",
        "//parser",
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
        "//base:status_macros",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_function_provider_test",
    srcs = [
        "cel_function_provider_test.cc",
    ],
    deps = [
        ":cel_function",
        ":cel_function_provider",
        "//base:status_macros",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cel_function_registry_test",
    srcs = [
        "cel_function_registry_test.cc",
    ],
    deps = [
        ":cel_function",
        ":cel_function_provider",
        ":cel_function_registry",
        "//base:status_macros",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
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
        "//base:status_macros",
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
        ":cel_function_registry",
        "//base:status_macros",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/status",
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
        ":cel_function_registry",
        ":cel_value",
        ":extension_func_registrar",
        "//base:status_macros",
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
    name = "unknown_attribute_set_test",
    size = "small",
    srcs = [
        "unknown_attribute_set_test.cc",
    ],
    deps = [
        ":cel_attribute",
        ":cel_value",
        ":unknown_attribute_set",
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
        ":value_export_util",
        "//base:status_macros",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "unknown_function_result_set",
    srcs = ["unknown_function_result_set.cc"],
    hdrs = ["unknown_function_result_set.h"],
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
    deps = [
        ":cel_function",
        ":cel_value",
        ":unknown_function_result_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "unknown_set",
    hdrs = ["unknown_set.h"],
    deps = [
        ":unknown_attribute_set",
        ":unknown_function_result_set",
    ],
)

cc_test(
    name = "unknown_set_test",
    srcs = ["unknown_set_test.cc"],
    deps = [
        ":cel_attribute",
        ":unknown_attribute_set",
        ":unknown_function_result_set",
        ":unknown_set",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "transform_utility",
    srcs = [
        "transform_utility.cc",
    ],
    hdrs = [
        "transform_utility.h",
    ],
    deps = [
        ":cel_value",
        "//base:status_macros",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:proto_util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:value_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "set_util",
    srcs = ["set_util.cc"],
    hdrs = ["set_util.h"],
    deps = ["//eval/public:cel_value"],
)

cc_test(
    name = "set_util_test",
    size = "small",
    srcs = [
        "set_util_test.cc",
    ],
    deps = [
        ":cel_value",
        ":set_util",
        ":unknown_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
