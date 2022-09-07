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
]

cc_binary(
    name = "server",
    testonly = 1,
    srcs = ["server.cc"],
    deps = [
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:transform_utility",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//internal:proto_util",
        "//parser",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_googleapis//google/api/expr/conformance/v1alpha1:conformance_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
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
            "--server=\"$(location :server) --base64_encode " + arg + "\"",
            "--skip_check",
            "--pipe",
            "--pipe_base64",

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
        "--server=\"$(location :server) --base64_encode\"",
        "--skip_check",
        # TODO(issues/119): Strong typing support for enums, specified but not implemented.
        "--skip_test=enums/strong_proto2",
        "--skip_test=enums/strong_proto3",
        "--pipe",
        "--pipe_base64",
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
