# Description
#   Common cel libraries
#
# Uses the namespace google::api::expr::common

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "macros",
    hdrs = [
        "macros.h",
    ],
    copts = ["-std=c++17"],
    deps = [],
)

cc_library(
    name = "id",
    hdrs = [
        "id.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//internal:cel_printer",
        "//internal:handle",
        "//internal:hash_util",
    ],
)

cc_library(
    name = "type",
    srcs = ["type.cc"],
    hdrs = [
        "type.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//internal:handle",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "enum",
    srcs = ["enum.cc"],
    hdrs = [
        "enum.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":type",
        "//internal:cel_printer",
        "//internal:ref_countable",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "error",
    srcs = ["error.cc"],
    hdrs = [
        "error.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//internal:cel_printer",
        "//internal:hash_util",
        "//internal:proto_util",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

cc_library(
    name = "unknown",
    srcs = ["unknown.cc"],
    hdrs = [
        "unknown.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":id",
        ":macros",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "parent_ref",
    srcs = [
        "parent_ref.cc",
    ],
    hdrs = [
        "parent_ref.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//internal:ref_countable",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "custom_object",
    srcs = [
        "custom_object.cc",
    ],
    hdrs = [
        "custom_object.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":value",
    ],
)

cc_library(
    name = "operators",
    srcs = [
        "operators.cc",
    ],
    hdrs = [
        "operators.h",
    ],
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "escaping_test",
    srcs = ["escaping_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":escaping",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "value",
    srcs = [
        "value.cc",
    ],
    hdrs = [
        "value.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":enum",
        ":error",
        ":id",
        ":macros",
        ":parent_ref",
        ":type",
        ":unknown",
        "//internal:cel_printer",
        "//internal:hash_util",
        "//internal:holder",
        "//internal:ref_countable",
        "//internal:status_util",
        "//internal:value_internal",
        "//internal:visitor_util",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_absl//absl/utility",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "value_test",
    srcs = ["value_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":custom_object",
        ":value",
        "//internal:status_util",
        "//internal:types",
        "//internal:value_internal",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc:status_cc_proto",
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
        ":parent_ref",
        ":value",
        "//internal:list_impl",
        "//internal:map_impl",
        "//internal:types",
    ],
)

cc_test(
    name = "converters_test",
    srcs = ["converters_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":converters",
        ":value",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
    ],
)
