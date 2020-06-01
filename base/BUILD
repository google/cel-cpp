licenses(["notice"])  # Apache v2.0

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "statusor",
    srcs = [
        "statusor.cc",
    ],
    hdrs = [
        "statusor.h",
        "statusor_internals.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = [
        "status_macros.h",
    ],
    copts = ["-std=c++17"],
)
