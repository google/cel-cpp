package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "flatbuffers_backed_impl",
    srcs = [
        "flatbuffers_backed_impl.cc",
    ],
    hdrs = [
        "flatbuffers_backed_impl.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "flatbuffers_backed_impl_test",
    size = "small",
    srcs = [
        "flatbuffers_backed_impl_test.cc",
    ],
    data = [
        "//tools/testdata:flatbuffers_reflection_out",
    ],
    deps = [
        ":flatbuffers_backed_impl",
        "//internal:status_macros",
        "//internal:testing",
        "@com_github_google_flatbuffers//:flatbuffers",
    ],
)
