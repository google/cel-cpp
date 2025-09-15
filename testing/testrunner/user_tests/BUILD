load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//testing/testrunner:cel_cc_test.bzl", "cel_cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "simple_user_test",
    testonly = True,
    srcs = ["simple.cc"],
    deps = [
        "//checker:type_checker_builder",
        "//checker:validation_result",
        "//common:ast_proto",
        "//common:decl",
        "//common:type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "//testing/testrunner:cel_expression_source",
        "//testing/testrunner:cel_test_context",
        "//testing/testrunner:cel_test_factories",
        "//testing/testrunner:default_result_matcher",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)

cc_library(
    name = "raw_expression_user_test",
    testonly = True,
    srcs = ["raw_expression_test.cc"],
    deps = [
        "//checker:type_checker_builder",
        "//common:decl",
        "//common:type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "//testing/testrunner:cel_expression_source",
        "//testing/testrunner:cel_test_context",
        "//testing/testrunner:cel_test_factories",
        "//testing/testrunner:default_result_matcher",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cel_cc_test(
    name = "simple_test",
    filegroup = "//testing/testrunner/resources",
    test_data_path = "//testing/testrunner/resources",
    test_suite = "simple_tests.textproto",
    deps = [
        ":simple_user_test",
    ],
)

cel_cc_test(
    name = "simple_test_with_custom_test_suite",
    filegroup = "//testing/testrunner/resources",
    test_data_path = "//testing/testrunner/resources",
    deps = [
        ":simple_user_test",
    ],
)

cel_cc_test(
    name = "raw_expression_test_with_custom_test_suite",
    deps = [
        ":raw_expression_user_test",
    ],
)
