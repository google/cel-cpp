# Description
#   Libraries for working with protobuffer messages and well-known types.
#
# Uses the namespace google::api::expr::protoutil.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "type_registry",
    srcs = ["type_registry.cc"],
    hdrs = [
        "type_registry.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//common:macros",
        "//common:parent_ref",
        "//common:type",
        "//common:value",
        "//internal:cast",
        "//internal:map_impl",
        "//internal:proto_util",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "type_registry_test",
    srcs = ["type_registry_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":type_registry",
        "//common:value",
        "//internal:ref_countable",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_googleapis//google/type:money_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

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
        ":type_registry",
        "//common:macros",
        "//common:parent_ref",
        "//common:value",
        "//internal:holder",
        "//internal:list_impl",
        "//internal:map_impl",
        "//internal:proto_util",
        "//internal:ref_countable",
        "//internal:status_util",
        "//internal:types",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
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
        ":type_registry",
        "//common:value",
        "//testutil:test_data_io",
        "//testutil:test_data_util",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_cel_spec//testdata:test_data_cc_proto",
        "@com_google_cel_spec//testdata:test_value_cc_proto",
        "@com_google_googleapis//google/type:money_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
