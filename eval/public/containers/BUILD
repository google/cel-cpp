# Container type implementations for use in the c++ CEL evaluator.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

# TODO(issues/69): Expose this in a public API.

cc_library(
    name = "field_access",
    srcs = [
        "field_access.cc",
    ],
    hdrs = [
        "field_access.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:proto_util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "container_backed_list_impl",
    srcs = [
    ],
    hdrs = [
        "container_backed_list_impl.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "container_backed_map_impl",
    srcs = [
        "container_backed_map_impl.cc",
    ],
    hdrs = [
        "container_backed_map_impl.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "field_backed_list_impl",
    srcs = [
        "field_backed_list_impl.cc",
    ],
    hdrs = [
        "field_backed_list_impl.h",
    ],
    deps = [
        ":field_access",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "field_backed_map_impl",
    srcs = [
        "field_backed_map_impl.cc",
    ],
    hdrs = [
        "field_backed_map_impl.h",
    ],
    deps = [
        ":field_access",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "container_backed_map_impl_test",
    size = "small",
    srcs = [
        "container_backed_map_impl_test.cc",
    ],
    deps = [
        ":container_backed_map_impl",
        "//eval/public:cel_value",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "field_backed_list_impl_test",
    size = "small",
    srcs = [
        "field_backed_list_impl_test.cc",
    ],
    deps = [
        ":field_backed_list_impl",
        "//eval/eval:evaluator_core",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "field_backed_map_impl_test",
    size = "small",
    srcs = [
        "field_backed_map_impl_test.cc",
    ],
    deps = [
        ":field_backed_map_impl",
        "//eval/eval:evaluator_core",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
