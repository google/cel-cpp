# Description
#   Implementation of the conformance test server

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

ALL_TESTS = [
    "@com_google_cel_spec//tests/simple:testdata/basic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/comparisons.textproto",
    "@com_google_cel_spec//tests/simple:testdata/conversions.textproto",
    "@com_google_cel_spec//tests/simple:testdata/dynamic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/enums.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fields.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fp_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/integer_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/lists.textproto",
    "@com_google_cel_spec//tests/simple:testdata/logic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/macros.textproto",
    "@com_google_cel_spec//tests/simple:testdata/namespace.textproto",
    "@com_google_cel_spec//tests/simple:testdata/plumbing.textproto",
    # TODO(issues/92): Support for parse-only proto message creation within a container.
    # "@com_google_cel_spec//tests/simple:testdata/proto2.textproto",
    # "@com_google_cel_spec//tests/simple:testdata/proto3.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string.textproto",
    "@com_google_cel_spec//tests/simple:testdata/timestamps.textproto",
    "@com_google_cel_spec//tests/simple:testdata/unknowns.textproto",
]

DASHBOARD_TESTS = [
    "@com_google_cel_spec//tests/simple:testdata/basic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/comparisons.textproto",
    "@com_google_cel_spec//tests/simple:testdata/conversions.textproto",
    "@com_google_cel_spec//tests/simple:testdata/dynamic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/enums.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fields.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fp_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/integer_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/lists.textproto",
    "@com_google_cel_spec//tests/simple:testdata/logic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/macros.textproto",
    "@com_google_cel_spec//tests/simple:testdata/namespace.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto2.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto3.textproto",
    "@com_google_cel_spec//tests/simple:testdata/plumbing.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string.textproto",
    "@com_google_cel_spec//tests/simple:testdata/timestamps.textproto",
    "@com_google_cel_spec//tests/simple:testdata/unknowns.textproto",
]

cc_binary(
    name = "server",
    testonly = 1,
    srcs = ["server.cc"],
    deps = [
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:transform_utility",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//internal:proto_util",
        "//parser",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:conformance_service_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

[
    sh_test(
        name = "simple" + arg,
        srcs = ["@com_google_cel_spec//tests:conftest.sh"],
        args = [
            "$(location @com_google_cel_spec//tests/simple:simple_test)",
            "--server=\"$(location :server) " + arg + "\"",
            "--skip_check",
            "--pipe",
            # TODO(issues/93): Inconsistent Duration.getMilliseconds() behavior.
            "--skip_test=timestamps/duration_converters/get_milliseconds",
            # TODO(issues/94): Missing timestamp conversion functions (type / string)
            "--skip_test=timestamps/timestamp_conversions/toType_timestamp,toString_timestamp",
            "--skip_test=timestamps/duration_conversions/toType_duration,toString_duration",
            # TODO(issues/78): Missing bytes() conversion functions
            "--skip_test=conversions/bytes",
            # TODO(issues/79): Missing double() conversion functions
            "--skip_test=conversions/double",
            # TODO(issues/80): Missing dyn() conversion functions
            "--skip_test=conversions/dyn/dyn_heterogeneous_list",
            # TODO(issues/81): Conversion functions for int() which can be
            # uncommented when the spec changes to truncation rather than
            # rounding.
            "--skip_test=conversions/int/double_nearest,double_nearest_neg,double_half_away_neg,double_half_away_pos",
            # TODO(issues/82): Unexpected behavior when converting invalid bytes to string.
            "--skip_test=conversions/string/bytes_invalid",
            # TODO(issues/83): Missing type() conversion functions
            "--skip_test=conversions/type",
            # TODO(issues/84): Missing uint() conversion functions
            "--skip_test=conversions/uint",
            # TODO(issues/96): Well-known type conversion support.
            "--skip_test=proto2/literal_wellknown",
            "--skip_test=proto3/literal_wellknown",
            "--skip_test=proto2/empty_field/wkt",
            "--skip_test=proto3/empty_field/wkt",
            # TODO(issues/92): Support for parse-only proto message creation within a container.
            "--skip_test=proto2/has/undefined",
            "--skip_test=proto3/has/undefined",
            # Requires container support
            "--skip_test=namespace/namespace/self_eval_container_lookup_unchecked",
            "--skip_test=basic/self_eval_nonzeroish/self_eval_bytes_invalid_utf8",
            # Requires heteregenous equality spec clarification
            "--skip_test=comparisons/eq_literal/eq_bytes",
            "--skip_test=comparisons/ne_literal/not_ne_bytes",
            "--skip_test=comparisons/in_list_literal/elem_in_mixed_type_list_error",
            "--skip_test=comparisons/in_map_literal/key_in_mixed_key_type_map_error",
            "--skip_test=fields/in/singleton",
            # Requires qualified bindings error message relaxation
            "--skip_test=fields/qualified_identifier_resolution/int64_field_select_unsupported,list_field_select_unsupported,map_key_null,qualified_identifier_resolution_unchecked",
            "--skip_test=string/size/one_unicode,unicode",
            "--skip_test=string/bytes_concat/left_unit",
            # TODO(issues/85): The exists one macro should not short-circuit false.
            "--skip_test=macros/exists_one/list_no_shortcircuit",
            # TODO(issues/86): Map macro may produce incorrect results on error.
            "--skip_test=macros/map/list_error",
            # TODO(issues/97): Parse-only qualified variable lookup "x.y" wtih binding "x.y" or "y" within container "x" fails
            "--skip_test=namespace/qualified/self_eval_qualified_lookup",
            "--skip_test=namespace/namespace/self_eval_container_lookup",
            "--skip_test=fields/qualified_identifier_resolution/qualified_ident",
            "--skip_test=fields/qualified_identifier_resolution/map_field_select",
            "--skip_test=fields/qualified_identifier_resolution/ident_with_longest_prefix_check",
        ] + ["$(location " + test + ")" for test in ALL_TESTS],
        data = [
            ":server",
            "@com_google_cel_spec//tests/simple:simple_test",
        ] + ALL_TESTS,
    )
    for arg in [
        "",
        "--opt",
    ]
]

sh_test(
    name = "simple-dashboard-test.sh",
    srcs = ["@com_google_cel_spec//tests:conftest-nofail.sh"],
    args = [
        "$(location @com_google_cel_spec//tests/simple:simple_test)",
        "--server=$(location :server)",
        "--skip_check",
        "--pipe",
    ] + ["$(location " + test + ")" for test in DASHBOARD_TESTS],
    data = [
        ":server",
        "@com_google_cel_spec//tests/simple:simple_test",
    ] + DASHBOARD_TESTS,
    visibility = [
        "//:__subpackages__",
        "//third_party/cel:__pkg__",
    ],
)
