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
    "@com_google_cel_spec//tests/simple:testdata/parse.textproto",
    "@com_google_cel_spec//tests/simple:testdata/plumbing.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto2.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto3.textproto",
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
        "@com_google_absl//absl/flags:flag",
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

            # Tests which require spec changes.
            # TODO(issues/93): Deprecate Duration.getMilliseconds.
            "--skip_test=timestamps/duration_converters/get_milliseconds",
            # TODO(issues/81): Conversion functions for int(), uint() which can be
            # uncommented when the spec changes to truncation rather than rounding.
            "--skip_test=conversions/int/double_nearest,double_nearest_neg,double_half_away_neg,double_half_away_pos",
            "--skip_test=conversions/uint/double_nearest,double_nearest_int,double_half_away",
            # TODO(issues/110): Tune parse limits to mirror those for proto deserialization and C++ safety limits.
            "--skip_test=parse/nest/list_index,message_literal,funcall,list_literal,map_literal;repeat/conditional,add_sub,mul_div,select,index,map_literal,message_literal",

            # Broken test cases which should be supported.
            # TODO(issues/111): Byte literal decoding of invalid UTF-8 results in incorrect bytes output.
            "--skip_test=basic/self_eval_nonzeroish/self_eval_bytes_invalid_utf8",
            "--skip_test=string/bytes_concat/left_unit",
            # TODO(issues/112): Unbound functions result in empty eval response.
            "--skip_test=basic/functions/unbound",
            "--skip_test=basic/functions/unbound_is_runtime_error",
            # TODO(issues/113): Aggregate values must logically AND element equality results.
            "--skip_test=comparisons/eq_literal/not_eq_list_false_vs_types",
            "--skip_test=comparisons/eq_literal/not_eq_map_false_vs_types",
            # TODO(issues/114): Ensure the 'in' operator is a logical OR of element equality results.
            "--skip_test=comparisons/in_list_literal/elem_in_mixed_type_list_error",
            "--skip_test=comparisons/in_map_literal/key_in_mixed_key_type_map_error",
            # TODO(issues/97): Parse-only qualified variable lookup "x.y" wtih binding "x.y" or "y" within container "x" fails
            "--skip_test=fields/qualified_identifier_resolution/qualified_ident,map_field_select,ident_with_longest_prefix_check,qualified_identifier_resolution_unchecked",
            "--skip_test=namespace/qualified/self_eval_qualified_lookup",
            "--skip_test=namespace/namespace/self_eval_container_lookup,self_eval_container_lookup_unchecked",
            # TODO(issues/116): Debug why dynamic/list/var fails to JSON parse correctly.
            "--skip_test=dynamic/list/var",
            # TODO(issues/109): Ensure that unset wrapper fields return 'null' rather than the default value of the wrapper.
            "--skip_test=dynamic/int32/field_read_proto2_unset,field_read_proto3_unset;uint32/field_read_proto2_unset;uint64/field_read_proto2_unset;float/field_read_proto2_unset,field_read_proto3_unset;double/field_read_proto2_unset,field_read_proto3_unset",
            "--skip_test=proto2/empty_field/wkt",
            "--skip_test=proto3/empty_field/wkt",
            # TODO(issues/117): Integer overflow on enum assignments should error.
            "--skip_test=enums/legacy_proto2/select_big,select_neg,assign_standalone_int_too_big,assign_standalone_int_too_neg",
            "--skip_test=enums/legacy_proto3/assign_standalone_int_too_big,assign_standalone_int_too_neg",
            # TODO(issues/118): Duration and timestamp range errors should result in evaluation errors.
            "--skip_test=timestamps/timestamp_range",

            # Future features for CEL 1.0
            # TODO(issues/119): Strong typing support for enums, specified but not implemented.
            "--skip_test=enums/strong_proto2",
            "--skip_test=enums/strong_proto3",
            # Bad tests, temporarily disable.
            "--skip_test=dynamic/float/field_assign_proto2_range,field_assign_proto3_range",
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
        # TODO(issues/116): Debug why dynamic/list/var fails to JSON parse correctly.
        "--skip_test=dynamic/list/var",
        # TODO(issues/119): Strong typing support for enums, specified but not implemented.
        "--skip_test=enums/strong_proto2",
        "--skip_test=enums/strong_proto3",
        "--pipe",
    ] + ["$(location " + test + ")" for test in ALL_TESTS],
    data = [
        ":server",
        "@com_google_cel_spec//tests/simple:simple_test",
    ] + ALL_TESTS,
    visibility = [
        "//:__subpackages__",
        "//third_party/cel:__pkg__",
    ],
)
