# Description
#   Internal implemenation details and libraries.
#
# Uses the namespace google::api::expr::internal

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "benchmark",
    testonly = True,
    hdrs = ["benchmark.h"],
    deps = [
        "@com_github_google_benchmark//:benchmark_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "casts",
    hdrs = ["casts.h"],
)

cc_library(
    name = "status_builder",
    hdrs = ["status_builder.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
    deps = [
        ":status_builder",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "proto_util",
    srcs = ["proto_util.cc"],
    hdrs = ["proto_util.h"],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "testing",
    testonly = True,
    srcs = [
        "testing.cc",
    ],
    hdrs = [
        "testing.h",
    ],
    deps = [
        ":status_builder",
        ":status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "utf8",
    srcs = ["utf8.cc"],
    hdrs = ["utf8.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "utf8_test",
    srcs = ["utf8_test.cc"],
    deps = [
        ":benchmark",
        ":testing",
        ":utf8",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)
