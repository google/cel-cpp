# Copyright 2024 Google LLC
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

cc_library(
    name = "compiler",
    hdrs = ["compiler.h"],
    deps = [
        "//checker:checker_options",
        "//checker:type_checker_builder",
        "//checker:validation_result",
        "//parser:options",
        "//parser:parser_interface",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "compiler_factory",
    srcs = ["compiler_factory.cc"],
    hdrs = ["compiler_factory.h"],
    deps = [
        ":compiler",
        "//checker:type_checker",
        "//checker:type_checker_builder",
        "//checker:validation_result",
        "//common:source",
        "//internal:noop_delete",
        "//internal:status_macros",
        "//parser",
        "//parser:parser_interface",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "compiler_factory_test",
    srcs = ["compiler_factory_test.cc"],
    deps = [
        ":compiler",
        ":compiler_factory",
        "//checker:optional",
        "//checker:standard_library",
        "//checker:type_check_issue",
        "//checker:validation_result",
        "//common:decl",
        "//common:type",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//parser:macro",
        "//parser:parser_interface",
        "//testutil:baseline_tests",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_protobuf//:protobuf",
    ],
)
