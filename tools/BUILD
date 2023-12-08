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

cc_library(
    name = "navigable_ast",
    srcs = ["navigable_ast.cc"],
    hdrs = ["navigable_ast.h"],
    deps = [
        "//eval/public:ast_traverse",
        "//eval/public:ast_visitor",
        "//eval/public:ast_visitor_base",
        "//eval/public:source_position",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "navigable_ast_test",
    srcs = ["navigable_ast_test.cc"],
    deps = [
        ":navigable_ast",
        "//base:builtins",
        "//internal:testing",
        "//parser",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)
