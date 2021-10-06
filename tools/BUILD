load(
    "@com_github_google_flatbuffers//:build_defs.bzl",
    "flatbuffer_library_public",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

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

flatbuffer_library_public(
    name = "flatbuffers_test",
    srcs = ["testdata/flatbuffers.fbs"],
    outs = ["testdata/flatbuffers_generated.h"],
    language_flag = "-c",
    reflection_name = "flatbuffers_reflection",
)

cc_library(
    name = "flatbuffers_test_cc",
    srcs = [":flatbuffers_test"],
    hdrs = [":flatbuffers_test"],
    features = ["-parse_headers"],
    linkstatic = True,
    deps = ["@com_github_google_flatbuffers//:runtime_cc"],
)

cc_test(
    name = "flatbuffers_backed_impl_test",
    size = "small",
    srcs = [
        "flatbuffers_backed_impl_test.cc",
    ],
    data = [
        ":flatbuffers_reflection_out",
    ],
    deps = [
        ":flatbuffers_backed_impl",
        "//internal:status_macros",
        "//internal:testing",
        "@com_github_google_flatbuffers//:flatbuffers",
    ],
)
