load(
    "@com_github_google_flatbuffers//:build_defs.bzl",
    "flatbuffer_library_public",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "flatbuffers_backed_impl",
    srcs = [
        "flatbuffers_backed_impl.cc",
    ],
    hdrs = [
        "flatbuffers_backed_impl.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "//eval/public:cel_value",
        "@com_github_google_flatbuffers//:flatbuffers",
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
    copts = ["-std=c++17"],
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
    copts = ["-std=c++17"],
    data = [
        ":flatbuffers_reflection_out",
    ],
    deps = [
        ":flatbuffers_backed_impl",
        ":flatbuffers_test_cc",
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_github_google_googletest//:gtest_main",
    ],
)
