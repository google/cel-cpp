package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "cel_proto_wrapper",
    srcs = [
        "cel_proto_wrapper.cc",
    ],
    hdrs = [
        "cel_proto_wrapper.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "//internal:proto_util",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_proto_wrapper_test",
    size = "small",
    srcs = [
        "cel_proto_wrapper_test.cc",
    ],
    deps = [
        ":cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:proto_util",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
