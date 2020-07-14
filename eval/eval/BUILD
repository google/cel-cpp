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
    copts = ["-std=c++17"],
    deps = [
        ":attribute_trail",
        ":unknowns_utility",
        "//base:status_macros",
        "//base:statusor",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "expression_step_base",
    hdrs = [
        "expression_step_base.h",
    ],
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "@com_google_absl//absl/status",
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
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
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
    copts = ["-std=c++17"],
    deps = [
        ":attribute_trail",
        ":evaluator_core",
        ":expression_build_warning",
        ":expression_step_base",
        "//base:status_macros",
        "//eval/public:activation",
        "//eval/public:cel_builtins",
        "//eval/public:cel_function",
        "//eval/public:cel_function_provider",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_value",
        "//eval/public:unknown_function_result_set",
        "//eval/public:unknown_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
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
    copts = ["-std=c++17"],
    deps = [
        "//eval/public:cel_value",
        "//internal:proto_util",
        "@com_google_absl//absl/status",
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        ":field_access",
        ":field_backed_list_impl",
        ":field_backed_map_impl",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":container_backed_map_impl",
        ":evaluator_core",
        ":expression_step_base",
        ":field_access",
        "@com_google_absl//absl/status",
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//eval/public:activation",
        "//eval/public:cel_function",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
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
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:status_macros",
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
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        "//base:status_macros",
        "//eval/compiler:flat_expr_builder",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "const_value_step_test",
    size = "small",
    srcs = [
        "const_value_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":const_value_step",
        ":evaluator_core",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "container_access_step_test",
    size = "small",
    srcs = [
        "container_access_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":container_access_step",
        ":container_backed_list_impl",
        ":container_backed_map_impl",
        ":ident_step",
        "//base:status_macros",
        "//eval/public:cel_attribute",
        "//eval/public:cel_builtins",
        "//eval/public:cel_value",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "ident_step_test",
    size = "small",
    srcs = [
        "ident_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":ident_step",
        "//base:status_macros",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "function_step_test",
    size = "small",
    srcs = [
        "function_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":expression_build_warning",
        ":function_step",
        ":ident_step",
        "//base:status_macros",
        "//eval/public:cel_attribute",
        "//eval/public:cel_function",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:unknown_function_result_set",
        "//eval/testutil:test_message_cc_proto",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "container_backed_map_impl_test",
    size = "small",
    srcs = [
        "container_backed_map_impl_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":container_backed_map_impl",
        "//eval/public:cel_value",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "field_backed_list_impl_test",
    size = "small",
    srcs = [
        "field_backed_list_impl_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":field_backed_list_impl",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "field_backed_map_impl_test",
    size = "small",
    srcs = [
        "field_backed_map_impl_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":evaluator_core",
        ":field_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "logic_step_test",
    size = "small",
    srcs = [
        "logic_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":ident_step",
        ":logic_step",
        "//base:status_macros",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "select_step_test",
    size = "small",
    srcs = [
        "select_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":container_backed_map_impl",
        ":ident_step",
        ":select_step",
        "//base:status_macros",
        "//eval/public:cel_attribute",
        "//eval/public:unknown_attribute_set",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "create_list_step_test",
    size = "small",
    srcs = [
        "create_list_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":const_value_step",
        ":create_list_step",
        ":ident_step",
        "//base:status_macros",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:unknown_attribute_set",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "create_struct_step_test",
    size = "small",
    srcs = [
        "create_struct_step_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":container_backed_list_impl",
        ":container_backed_map_impl",
        ":create_struct_step",
        ":ident_step",
        "//base:status_macros",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "expression_build_warning",
    srcs = [
        "expression_build_warning.cc",
    ],
    hdrs = [
        "expression_build_warning.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "expression_build_warning_test",
    size = "small",
    srcs = [
        "expression_build_warning_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":expression_build_warning",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "attribute_trail",
    srcs = ["attribute_trail.cc"],
    hdrs = ["attribute_trail.h"],
    copts = ["-std=c++17"],
    deps = [
        "//base:statusor",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "attribute_trail_test",
    size = "small",
    srcs = [
        "attribute_trail_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":attribute_trail",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "unknowns_utility",
    srcs = ["unknowns_utility.cc"],
    hdrs = ["unknowns_utility.h"],
    copts = ["-std=c++17"],
    deps = [
        ":attribute_trail",
        "//base:statusor",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_expression",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "unknowns_utility_test",
    size = "small",
    srcs = [
        "unknowns_utility_test.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":unknowns_utility",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)
