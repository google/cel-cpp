package(
    default_testonly = True,
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "debug_string",
    srcs = ["debug_string.cc"],
    hdrs = ["debug_string.h"],
    deps = [
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public:unknown_function_result_set",
        "//eval/public:unknown_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "debug_string_test",
    srcs = ["debug_string_test.cc"],
    deps = [
        ":debug_string",
        "//eval/public:cel_attribute",
        "//eval/public:cel_function",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_function_result_set",
        "//eval/public:unknown_set",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "matchers",
    srcs = ["matchers.cc"],
    hdrs = ["matchers.h"],
    deps = [
        ":debug_string",
        "//eval/public:cel_value",
        "//eval/public:set_util",
        "//eval/public:unknown_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "matchers_test",
    srcs = ["matchers_test.cc"],
    deps = [
        ":matchers",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
