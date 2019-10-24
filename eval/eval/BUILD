# This package contains implementation of expression evaluator
# internals.
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "evaluator_core",
    srcs = [
        "evaluator_core.cc",
    ],
    hdrs = [
        "evaluator_core.h",
    ],
    deps = [
        "//eval/public:activation",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "expression_step_base",
    hdrs = [
        "expression_step_base.h",
    ],
    deps = [
        ":evaluator_core",
        "//eval/public:activation",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "const_value_step",
    srcs = [
        "const_value_step.cc",
    ],
    hdrs = [
        "const_value_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "container_access_step",
    srcs = [
        "container_access_step.cc",
    ],
    hdrs = [
        "container_access_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:status",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ident_step",
    srcs = [
        "ident_step.cc",
    ],
    hdrs = [
        "ident_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "function_step",
    srcs = [
        "function_step.cc",
    ],
    hdrs = [
        "function_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_function",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "field_access",
    srcs = [
        "field_access.cc",
    ],
    hdrs = [
        "field_access.h",
    ],
    deps = [
        "//base:status",
        "//eval/public:cel_value",
        "//internal:proto_util",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "container_backed_list_impl",
    srcs = [
    ],
    hdrs = [
        "container_backed_list_impl.h",
    ],
    # Make private once temporary internal use over.
    visibility = [
        "//conformance:__subpackages__",
        "//eval:__subpackages__",
        "//identity/cloud/attributecalculator:__subpackages__",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "container_backed_map_impl",
    srcs = [
        "container_backed_map_impl.cc",
    ],
    hdrs = [
        "container_backed_map_impl.h",
    ],
    # Make private once temporary internal use over.
    visibility = [
        "//conformance:__subpackages__",
        "//eval:__subpackages__",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "field_backed_list_impl",
    srcs = [
        "field_backed_list_impl.cc",
    ],
    hdrs = [
        "field_backed_list_impl.h",
    ],
    deps = [
        ":field_access",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "field_backed_map_impl",
    srcs = [
        "field_backed_map_impl.cc",
    ],
    hdrs = [
        "field_backed_map_impl.h",
    ],
    deps = [
        ":field_access",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "select_step",
    srcs = [
        "select_step.cc",
    ],
    hdrs = [
        "select_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        ":field_access",
        ":field_backed_list_impl",
        ":field_backed_map_impl",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "create_list_step",
    srcs = [
        "create_list_step.cc",
    ],
    hdrs = [
        "create_list_step.h",
    ],
    deps = [
        ":container_backed_list_impl",
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "create_struct_step",
    srcs = [
        "create_struct_step.cc",
    ],
    hdrs = [
        "create_struct_step.h",
    ],
    deps = [
        ":container_backed_map_impl",
        ":evaluator_core",
        ":expression_step_base",
        ":field_access",
        "//base:status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "jump_step",
    srcs = [
        "jump_step.cc",
    ],
    hdrs = [
        "jump_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "logic_step",
    srcs = [
        "logic_step.cc",
    ],
    hdrs = [
        "logic_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_function",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "comprehension_step",
    srcs = [
        "comprehension_step.cc",
    ],
    hdrs = [
        "comprehension_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_function",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "evaluator_core_test",
    size = "small",
    srcs = [
        "evaluator_core_test.cc",
    ],
    deps = [
        ":evaluator_core",
        "//eval/compiler:flat_expr_builder",
        "//eval/public:builtin_func_registrar",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "const_value_step_test",
    size = "small",
    srcs = [
        "const_value_step_test.cc",
    ],
    deps = [
        ":const_value_step",
        ":evaluator_core",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "container_access_step_test",
    size = "small",
    srcs = [
        "container_access_step_test.cc",
    ],
    deps = [
        ":container_access_step",
        ":container_backed_list_impl",
        ":container_backed_map_impl",
        ":ident_step",
        "//eval/public:cel_builtins",
        "//eval/public:cel_value",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "ident_step_test",
    size = "small",
    srcs = [
        "ident_step_test.cc",
    ],
    deps = [
        ":evaluator_core",
        ":ident_step",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "function_step_test",
    size = "small",
    srcs = [
        "function_step_test.cc",
    ],
    deps = [
        ":evaluator_core",
        ":function_step",
        "//eval/public:cel_function_adapter",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "container_backed_map_impl_test",
    size = "small",
    srcs = [
        "container_backed_map_impl_test.cc",
    ],
    deps = [
        ":container_backed_map_impl",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "field_backed_list_impl_test",
    size = "small",
    srcs = [
        "field_backed_list_impl_test.cc",
    ],
    deps = [
        ":evaluator_core",
        ":field_backed_list_impl",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "field_backed_map_impl_test",
    size = "small",
    srcs = [
        "field_backed_map_impl_test.cc",
    ],
    deps = [
        ":evaluator_core",
        ":field_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "select_step_test",
    size = "small",
    srcs = [
        "select_step_test.cc",
    ],
    deps = [
        ":container_backed_map_impl",
        ":evaluator_core",
        ":ident_step",
        ":select_step",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "create_list_step_test",
    size = "small",
    srcs = [
        "create_list_step_test.cc",
    ],
    deps = [
        ":const_value_step",
        ":create_list_step",
        ":evaluator_core",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "create_struct_step_test",
    size = "small",
    srcs = [
        "create_struct_step_test.cc",
    ],
    deps = [
        ":container_backed_list_impl",
        ":container_backed_map_impl",
        ":create_struct_step",
        ":ident_step",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
