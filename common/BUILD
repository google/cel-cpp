# Description
#   Common cel libraries
#
# Uses the namespace google::api::expr::common

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "operators",
    srcs = [
        "operators.cc",
    ],
    hdrs = [
        "operators.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "escaping",
    srcs = [
        "escaping.cc",
    ],
    hdrs = [
        "escaping.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "escaping_test",
    srcs = ["escaping_test.cc"],
    deps = [
        ":escaping",
        "//internal:testing",
    ],
)

cc_library(
    name = "overflow",
    srcs = ["overflow.cc"],
    hdrs = ["overflow.h"],
    deps = [
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "overflow_test",
    srcs = ["overflow_test.cc"],
    deps = [
        ":overflow",
        "//internal:testing",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)
