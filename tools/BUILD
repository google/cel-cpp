package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "cel_ast_renumber",
    srcs = ["cel_ast_renumber.cc"],
    hdrs = ["cel_ast_renumber.h"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "reference_inliner",
    srcs = [
        "reference_inliner.cc",
    ],
    hdrs = [
        "reference_inliner.h",
    ],
    deps = [
        ":cel_ast_renumber",
        "//eval/public:ast_rewrite",
        "//eval/public:ast_traverse",
        "//eval/public:ast_visitor_base",
        "//eval/public:source_position",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_googlesource_code_re2//:re2",
    ],
)

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
