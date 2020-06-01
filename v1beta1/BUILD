# Description
#   Libraries for working with the v1beta1 API.
#
# Uses the namespace google::api:expr::v1beta1.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "converters",
    srcs = [
        "converters.cc",
    ],
    hdrs = [
        "converters.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//common:converters",
        "//common:macros",
        "//common:value",
        "//internal:holder",
        "//internal:map_impl",
        "//internal:proto_util",
        "//internal:status_util",
        "//protoutil:type_registry",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1beta1:eval_cc_proto",
        "@com_google_googleapis//google/api/expr/v1beta1:value_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "converters_test",
    srcs = ["converters_test.cc"],
    copts = ["-std=c++17"],
    data = [
        "@com_google_cel_spec//testdata",
    ],
    deps = [
        ":converters",
        "//common:value",
        "//internal:status_util",
        "//protoutil:converters",
        "//protoutil:type_registry",
        "//testutil:test_data_io",
        "//testutil:test_data_util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_cel_spec//testdata:test_data_cc_proto",
        "@com_google_googleapis//google/type:money_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
