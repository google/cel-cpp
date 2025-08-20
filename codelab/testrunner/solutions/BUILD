load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//bazel:cel_cc_embed.bzl", "cel_cc_embed")
load("//third_party/cel/expr:cel_test.bzl", "cel_test", "make_test_targets", "test_runtime")

package(default_visibility = ["//visibility:public"])

cel_cc_embed(
    name = "basic_testsuite",
    src = "//codelab/testrunner/solutions/resources:basic_testsuite_binarypb",
)

cc_library(
    name = "basic_user_test",
    testonly = True,
    srcs = ["basic_test.cc"],
    textual_hdrs = [":basic_testsuite"],
    deps = [
        "//checker:type_checker_builder",
        "//checker:validation_result",
        "//common:ast_proto",
        "//common:decl",
        "//common:type",
        "//compiler",
        "//compiler:compiler_factory",
        "//compiler:standard_library",
        "//internal:empty_descriptors",
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//runtime",
        "//runtime:function_adapter",
        "//runtime:runtime_builder",
        "//runtime:standard_runtime_builder_factory",
        "//testing/testrunner:cel_test_context",
        "//testing/testrunner:cel_test_factories",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)

cel_test(
    name = "basic_cel_test",
    test_targets = [
        make_test_targets(
            runtime = test_runtime.CC,
            test_srcs = [":basic_user_test"],
        ),
    ],
)

cel_test(
    name = "basic_cel_test_with_resources",
    filegroup = "//codelab/testrunner/solutions/resources:data_files",
    test_data_path = "//codelab/testrunner/solutions/resources",
    test_suite = "basic_testsuite.textproto",
    test_targets = [
        make_test_targets(
            runtime = test_runtime.CC,
            test_srcs = [":basic_user_test"],
        ),
    ],
)
