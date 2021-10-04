# Description
#   Test utilities for cpp CEL.
#
# Uses the namespace google::api::expr::testutil.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "expr_printer",
    srcs = ["expr_printer.cc"],
    hdrs = ["expr_printer.h"],
    deps = [
        "//common:escaping",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "util",
    testonly = True,
    hdrs = [
        "util.h",
    ],
    deps = [
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)
