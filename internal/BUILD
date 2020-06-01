# Description
#   Internal implemenation details and libraries.
#
# Uses the namespace google::api::expr::internal

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "ref_countable",
    srcs = ["ref_countable.cc"],
    hdrs = [
        "ref_countable.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":holder",
        ":specialize",
    ],
)

cc_test(
    name = "ref_countable_test",
    srcs = ["ref_countable_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":ref_countable",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
    ],
)

cc_library(
    name = "handle",
    hdrs = [
        "handle.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":hash_util",
        ":specialize",
    ],
)

cc_test(
    name = "handle_test",
    srcs = ["handle_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":handle",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/container:node_hash_set",
    ],
)

cc_library(
    name = "holder",
    hdrs = [
        "holder.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":port",
        ":specialize",
        ":types",
        ":visitor_util",
    ],
)

cc_test(
    name = "holder_test",
    srcs = ["holder_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":holder",
        ":types",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "hash_util",
    srcs = [
        "hash_util.cc",
    ],
    hdrs = [
        "hash_util.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":port",
        ":specialize",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "status_util",
    srcs = ["status_util.cc"],
    hdrs = [
        "status_util.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

cc_library(
    name = "visitor_util",
    hdrs = [
        "visitor_util.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":specialize",
        ":types",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "visitor_util_test",
    srcs = ["visitor_util_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":adapter_util",
        ":visitor_util",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "adapter_util",
    hdrs = [
        "adapter_util.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":visitor_util",
    ],
)

cc_library(
    name = "cel_printer",
    srcs = ["cel_printer.cc"],
    hdrs = [
        "cel_printer.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":specialize",
        ":types",
        ":visitor_util",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_printer_test",
    srcs = ["cel_printer_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":cel_printer",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "proto_util",
    srcs = ["proto_util.cc"],
    hdrs = ["proto_util.h"],
    copts = ["-std=c++17"],
    deps = [
        ":status_util",
        "//common:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "map_impl",
    srcs = ["map_impl.cc"],
    hdrs = ["map_impl.h"],
    copts = ["-std=c++17"],
    deps = [
        ":status_util",
        "//common:macros",
        "//common:value",
        "@com_google_absl//absl/container:node_hash_map",
    ],
)

cc_library(
    name = "list_impl",
    hdrs = ["list_impl.h"],
    copts = ["-std=c++17"],
    deps = [
        ":holder",
        "//common:macros",
        "//common:value",
    ],
)

cc_library(
    name = "value_internal",
    hdrs = ["value_internal.h"],
    copts = ["-std=c++17"],
    deps = [
        ":adapter_util",
        ":cast",
        ":ref_countable",
        "//common:enum",
        "//common:error",
        "//common:id",
        "//common:parent_ref",
        "//common:type",
        "//common:unknown",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "value_internal_test",
    srcs = ["value_internal_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":value_internal",
        "//testutil:util",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "port",
    hdrs = ["port.h"],
    copts = ["-std=c++17"],
)

cc_library(
    name = "specialize",
    hdrs = ["specialize.h"],
    copts = ["-std=c++17"],
)

cc_library(
    name = "cast",
    hdrs = ["cast.h"],
    copts = ["-std=c++17"],
    deps = [
        ":port",
        ":specialize",
        ":types",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "cast_test",
    srcs = ["cast_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":cast",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "types",
    hdrs = [
        "types.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":port",
        ":specialize",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "types_test",
    srcs = ["types_test.cc"],
    copts = ["-std=c++17"],
    deps = [
        ":types",
        "@com_github_google_googletest//:gtest_main",
    ],
)
