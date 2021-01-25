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
    name = "test_data_util",
    srcs = [
        "test_data_util.cc",
    ],
    hdrs = [
        "test_data_util.h",
    ],
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
    testonly = True,
    srcs = [
        "test_data_io.cc",
    ],
    hdrs = [
        "test_data_io.h",
    ],
    deps = [
        "//internal:status_util",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//testdata:test_data_cc_proto",
        "@com_google_cel_spec//testdata:test_value_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_googletest//:gtest",
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
    testonly = True,
    srcs = [
        "test_data_gen.cc",
    ],
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
    data = [
        "@com_google_cel_spec//testdata",
    ],
    deps = [
        ":test_data_util",
        "//testutil:test_data_io",
        "@com_google_cel_spec//testdata:test_data_cc_proto",
        "@com_google_googleapis//google/api/expr/v1beta1:value_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "util",
    testonly = True,
    hdrs = [
        "util.h",
    ],
    deps = [
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)
