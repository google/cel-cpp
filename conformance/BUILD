# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

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
    "@com_google_cel_spec//tests/simple:testdata/wrappers.textproto",
]

cc_binary(
    name = "server",
    testonly = 1,
    srcs = [
        "server.cc",
        "value_conversion.cc",
        "value_conversion.h",
    ],
    deps = [
        "//base:data",
        "//base:handle",
        "//base:kind",
        "//base:memory",
        "//common:any",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:transform_utility",
        "//extensions/protobuf:data",
        "//extensions/protobuf:enum_adapter",
        "//extensions/protobuf:memory_manager",
        "//extensions/protobuf:runtime_adapter",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "//parser",
        "//runtime",
        "//runtime:activation",
        "//runtime:constant_folding",
        "//runtime:managed_value_factory",
        "//runtime:runtime_options",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_googleapis//google/api/expr/conformance/v1alpha1:conformance_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:value_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

[
    sh_test(
        name = "simple" + "".join([arg.replace("--", "_") for arg in args]),
        srcs = ["@com_google_cel_spec//tests:conftest.sh"],
        args = [
            "$(location @com_google_cel_spec//tests/simple:simple_test)",
            "--server=\"$(location :server) " + " ".join(args) + "\"",
            "--skip_check",
            "--pipe",
            "--pipe_pings",

            # Tests which require spec changes.
            # TODO(issues/93): Deprecate Duration.getMilliseconds.
            "--skip_test=timestamps/duration_converters/get_milliseconds",

            # Broken test cases which should be supported.
            # TODO(issues/112): Unbound functions result in empty eval response.
            "--skip_test=basic/functions/unbound",
            "--skip_test=basic/functions/unbound_is_runtime_error",

            # TODO(issues/97): Parse-only qualified variable lookup "x.y" wtih binding "x.y" or "y" within container "x" fails
            "--skip_test=fields/qualified_identifier_resolution/qualified_ident,map_field_select,ident_with_longest_prefix_check,qualified_identifier_resolution_unchecked",
            "--skip_test=namespace/qualified/self_eval_qualified_lookup",
            "--skip_test=namespace/namespace/self_eval_container_lookup,self_eval_container_lookup_unchecked",
            # TODO(issues/117): Integer overflow on enum assignments should error.
            "--skip_test=enums/legacy_proto2/select_big,select_neg",

            # Skip until fixed.
            "--skip_test=wrappers/field_mask/to_json",
            "--skip_test=wrappers/empty/to_json",
            "--skip_test=fields/qualified_identifier_resolution/map_value_repeat_key_heterogeneous",

            # Future features for CEL 1.0
            # TODO(issues/119): Strong typing support for enums, specified but not implemented.
            "--skip_test=enums/strong_proto2",
            "--skip_test=enums/strong_proto3",
        ] + ["$(location " + test + ")" for test in ALL_TESTS],
        data = [
            ":server",
            "@com_google_cel_spec//tests/simple:simple_test",
        ] + ALL_TESTS,
    )
    for args in [
        [],
        ["--opt"],
    ]
]

[
    sh_test(
        name = "simple" + "".join([arg.replace("--", "_") for arg in args]),
        srcs = ["@com_google_cel_spec//tests:conftest.sh"],
        args = [
            "$(location @com_google_cel_spec//tests/simple:simple_test)",
            "--server=\"$(location :server) " + " ".join(args) + "\"",
            "--skip_check",
            "--pipe",
            "--pipe_pings",

            # Tests which require spec changes.
            # TODO(issues/93): Deprecate Duration.getMilliseconds.
            "--skip_test=timestamps/duration_converters/get_milliseconds",

            # Broken test cases which should be supported.
            # TODO(issues/112): Unbound functions result in empty eval response.
            "--skip_test=basic/functions/unbound",
            "--skip_test=basic/functions/unbound_is_runtime_error",

            # TODO(issues/97): Parse-only qualified variable lookup "x.y" wtih binding "x.y" or "y" within container "x" fails
            "--skip_test=fields/qualified_identifier_resolution/qualified_ident,map_field_select,ident_with_longest_prefix_check,qualified_identifier_resolution_unchecked",
            "--skip_test=namespace/qualified/self_eval_qualified_lookup",
            "--skip_test=namespace/namespace/self_eval_container_lookup,self_eval_container_lookup_unchecked",
            # TODO(issues/117): Integer overflow on enum assignments should error.
            "--skip_test=enums/legacy_proto2/select_big,select_neg",

            # Skip until fixed.
            "--skip_test=wrappers/field_mask/to_json",
            "--skip_test=fields/qualified_identifier_resolution/map_value_repeat_key_heterogeneous",

            # Future features for CEL 1.0
            # TODO(issues/119): Strong typing support for enums, specified but not implemented.
            "--skip_test=enums/strong_proto2",
            "--skip_test=enums/strong_proto3",

            # TODO(uncreated-issue/62): edge cases modern type provider handles differently.
            # Nested Any comparsions when unpacking fails.
            # Hard to express in pure CEL, maybe need to update conformance.
            "--skip_test=comparisons/eq_wrapper/" + ",".join([
                "eq_proto2_any_unpack_bytewise_fallback_not_equal",
                "eq_proto3_any_unpack_bytewise_fallback_not_equal",
            ]),
            "--skip_test=comparisons/ne_literal/" + ",".join([
                "ne_proto2_any_unpack_bytewise_fallback",
                "ne_proto3_any_unpack_bytewise_fallback",
            ]),
            # float overflows
            "--skip_test=dynamic/float/" + ",".join([
                "field_assign_proto2_subnorm",
                "field_assign_proto3_subnorm#01",
                "field_assign_proto2_round_to_zero",
                "field_assign_proto3_round_to_zero",
                "field_assign_proto2_range",
                "field_assign_proto3_range",
            ]),
            # JSON types
            "--skip_test=dynamic/struct/" + ",".join([
                "var",
                "field_assign_proto2_bad",
                "field_read_proto2",
                "field_assign_proto3_bad",
                "field_read_proto3",
            ]),
            "--skip_test=dynamic/value_struct/" + ",".join([
                "var",
                "field_read_proto2",
                "field_read_proto3",
            ]),
            "--skip_test=dynamic/any/var",
            "--skip_test=dynamic/complex/any_list_map",
            # Strong vs weak enum typing
            "--skip_test=enums/legacy_proto2/" + ",".join([
                "select_default",
                "select",
                "field_type",
            ]),
            "--skip_test=enums/legacy_proto3/" + ",".join([
                "select_default",
                "select",
                "field_type",
                "select_big",
                "select_neg",
                "field_type",
            ]),
            "--skip_test=proto2/literal_singular/float",
            "--skip_test=proto3/literal_singular/float",
        ] + ["$(location " + test + ")" for test in ALL_TESTS],
        data = [
            ":server",
            "@com_google_cel_spec//tests/simple:simple_test",
        ] + ALL_TESTS,
    )
    for args in [
        [
            "--modern",
            "--arena",
        ],
        [
            "--modern",
            "--arena",
            "--opt",
        ],
    ]
]

[
    sh_test(
        name = "simple_dashboard" + "".join([arg.replace("--", "_") for arg in args]),
        srcs = ["@com_google_cel_spec//tests:conftest-nofail.sh"],
        args = [
            "$(location @com_google_cel_spec//tests/simple:simple_test)",
            "--server=\"$(location :server) " + " ".join(args) + "\"",
            "--skip_check",
            # TODO(issues/119): Strong typing support for enums, specified but not implemented.
            "--skip_test=enums/strong_proto2",
            "--skip_test=enums/strong_proto3",
            "--pipe",
            "--pipe_pings",
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
    for args in [
        [],
        [
            "--modern",
            "--arena",
        ],
        [
            "--modern",
            "--noarena",
        ],
    ]
]
