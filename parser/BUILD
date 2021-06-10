load("//bazel:antlr.bzl", "antlr_cc_library")

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])  # Apache 2.0

antlr_cc_library(
    name = "cel",
    src = "Cel.g4",
)

cc_library(
    name = "parser",
    srcs = [
        "parser.cc",
    ],
    hdrs = [
        "parser.h",
    ],
    copts = [
        "-fexceptions",
    ],
    deps = [
        ":cel_cc_parser",
        ":macro",
        ":options",
        ":source_factory",
        ":visitor",
        "@antlr4_runtimes//:cpp",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "macro",
    srcs = [
        "macro.cc",
    ],
    hdrs = [
        "macro.h",
    ],
    copts = [
        "-fexceptions",
    ],
    deps = [
        ":source_factory",
        "//common:operators",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "visitor",
    srcs = [
        "balancer.cc",
        "visitor.cc",
    ],
    hdrs = [
        "balancer.h",
        "visitor.h",
    ],
    copts = [
        "-fexceptions",
    ],
    deps = [
        ":cel_cc_parser",
        ":macro",
        ":source_factory",
        "//common:escaping",
        "//common:operators",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "source_factory",
    srcs = [
        "source_factory.cc",
    ],
    hdrs = [
        "source_factory.h",
    ],
    copts = [
        "-fexceptions",
    ],
    deps = [
        ":cel_cc_parser",
        "//common:operators",
        "@antlr4_runtimes//:cpp",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "options",
    hdrs = ["options.h"],
)

cc_test(
    name = "parser_test",
    srcs = ["parser_test.cc"],
    deps = [
        ":options",
        ":parser",
        ":source_factory",
        "//testutil:expr_printer",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)
