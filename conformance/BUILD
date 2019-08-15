# Description
#   Implementation of the conformance test server

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

ALL_TESTS = [
    # Requires container support
    #"@com_google_cel_spec//tests/simple:testdata/basic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fp_math.textproto",
    # Overflow is not handled
    # "@com_google_cel_spec//tests/simple:testdata/integer_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/logic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/plumbing.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string.textproto",
    # Requires heteregenous equality spec clarification
    #"@com_google_cel_spec//tests/simple:testdata/comparisons.textproto",
    # Requires qualified bindings error message relaxation
    #"@com_google_cel_spec//tests/simple:testdata/fields.textproto",
]

DASHBOARD_TESTS = [
    "@com_google_cel_spec//tests/simple:testdata/basic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fp_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/integer_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/logic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/plumbing.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string.textproto",
    "@com_google_cel_spec//tests/simple:testdata/comparisons.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fields.textproto",
]

cc_binary(
    name = "server",
    testonly = 1,
    srcs = ["server.cc"],
    deps = [
        "//base:status",
        "//eval/eval:container_backed_list_impl",
        "//eval/eval:container_backed_map_impl",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:conformance_service_cc_grpc",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

[
    sh_test(
        name = "simple-" + driver,
        srcs = [":" + driver],
        args = [
            "$(location @com_google_cel_spec//tests/simple:simple_test)",
            "--server=$(location @com_google_cel_go//server/main:cel_server)",
            "--eval_server=$(location :server)",
        ] + ["$(location " + test + ")" for test in ALL_TESTS],
        data = [
            ":server",
            "@com_google_cel_go//server/main:cel_server",
            "@com_google_cel_spec//tests/simple:simple_test",
        ] + ALL_TESTS,
    )
    for driver in [
        "test.sh",
        "opt-test.sh",
    ]
]

sh_test(
    name = "simple-dashboard-test.sh",
    srcs = ["@com_google_cel_spec//tests:conftest-nofail.sh"],
    args = [
        "$(location @com_google_cel_spec//tests/simple:simple_test)",
        "--server=$(location @com_google_cel_go//server/main:cel_server)",
        "--eval_server=$(location :server)",
    ] + ["$(location " + test + ")" for test in DASHBOARD_TESTS],
    data = [
        ":server",
        "@com_google_cel_go//server/main:cel_server",
        "@com_google_cel_spec//tests/simple:simple_test",
    ] + DASHBOARD_TESTS,
)
