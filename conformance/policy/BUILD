# Copyright 2026 Google LLC
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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load(
    "//conformance/policy:policy_conformance_test.bzl",
    "cel_policy_conformance_test",
)

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "policy_conformance_test_lib",
    testonly = True,
    srcs = ["policy_conformance_test.cc"],
    deps = [
        "//common:ast",
        "//common:source",
        "//common:value",
        "//common/internal:value_conversion",
        "//compiler",
        "//env",
        "//env:config",
        "//env:env_runtime",
        "//env:env_std_extensions",
        "//env:env_yaml",
        "//env:runtime_std_extensions",
        "//extensions/protobuf:enum_adapter",
        "//internal:runfiles",
        "//internal:status_macros",
        "//internal:testing_descriptor_pool",
        "//internal:testing_no_main",
        "//policy:cel_policy",
        "//policy:cel_policy_parser",
        "//policy:cel_policy_validation_result",
        "//policy:compiler",
        "//policy:test_util",
        "//policy:yaml_policy_parser",
        "//runtime",
        "//runtime:activation",
        "//runtime:function_adapter",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:suite_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)

cel_policy_conformance_test(
    name = "policy_conformance_test",
    skip_tests = [
        # TODO(b/506179116): Fix these.
        # Need to add k8s custom yaml parser and mock runtime.
        "k8s",
        # Need to add support for context proto from yaml.
        "context_pb",
    ],
    test_files = [
        "@cel_policy//conformance:testdata",
    ],
)
