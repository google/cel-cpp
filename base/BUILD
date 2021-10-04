licenses(["notice"])  # Apache v2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "testing",
    testonly = True,
    srcs = [
        "testing.cc",
    ],
    hdrs = [
        "testing.h",
    ],
    deps = [
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = [
        "status_macros.h",
    ],
)

cc_library(
    name = "unilib",
    srcs = [
        "unilib.cc",
    ],
    hdrs = [
        "unilib.h",
    ],
    deps = [
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_google_absl//absl/strings",
    ],
)
