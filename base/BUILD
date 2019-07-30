licenses(["notice"])  # Apache v2.0

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "status",
    srcs = [
        "canonical_errors.cc",
        "status.cc",
        "statusor.cc",
    ],
    hdrs = [
        "canonical_errors.h",
        "status.h",
        "statusor.h",
        "statusor_internals.h",
    ],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)
