# Description
#   Test utilities for cpp CEL.
#
# Uses the namespace google::api::expr::testutil.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "test_data_util",
    srcs = [
        "test_data_util.cc",
    ],
    hdrs = [
        "test_data_util.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//common:type",
        "//common:value",
        "//internal:cel_printer",
        "//internal:proto_util",
        "//internal:types",
        "//protoutil:converters",
        "//protoutil:type_registry",
        "//v1beta1:converters",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//testdata:test_value_cc_proto",
        "@com_google_googleapis//google/api/expr/v1beta1:eval_cc_proto",
        "@com_google_googleapis//google/api/expr/v1beta1:value_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "test_data_io",
    srcs = [
        "test_data_io.cc",
    ],
    hdrs = [
        "test_data_io.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//internal:status_util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//testdata:test_data_cc_proto",
        "@com_google_cel_spec//testdata:test_value_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

# Usage:
#    blaze build testutil:test_data_gen &&
#        blaze-bin/testutil/test_data_gen >>
#        third_party/cel/spec/testdata/unique_values.textpb
cc_binary(
    name = "test_data_gen",
    srcs = [
        "test_data_gen.cc",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":test_data_io",
        ":test_data_util",
        "//common:type",
        "//internal:proto_util",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//testdata:test_data_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/type:money_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "test_data_test",
    srcs = ["test_data_test.cc"],
    copts = ["-std=c++17"],
    data = [
        "@com_google_cel_spec//testdata",
    ],
    deps = [
        ":test_data_util",
        "//testutil:test_data_io",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_cel_spec//testdata:test_data_cc_proto",
        "@com_google_googleapis//google/api/expr/v1beta1:value_cc_proto",
    ],
)

cc_library(
    name = "util",
    hdrs = [
        "util.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "@com_github_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
