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
        "//common:overflow",
        "//eval/public:cel_value",
        "//eval/testutil:test_message_cc_proto",
        "//internal:proto_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
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
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "//internal:proto_util",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)
