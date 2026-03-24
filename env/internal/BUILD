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
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "ext_registry",
    srcs = ["ext_registry.cc"],
    hdrs = ["ext_registry.h"],
    deps = [
        "//compiler",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "runtime_ext_registry",
    srcs = ["runtime_ext_registry.cc"],
    hdrs = ["runtime_ext_registry.h"],
    deps = [
        "//runtime:runtime_builder",
        "//runtime:runtime_options",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "ext_registry_test",
    srcs = ["ext_registry_test.cc"],
    deps = [
        ":ext_registry",
        "//checker:type_checker_builder",
        "//compiler",
        "//internal:testing",
        "//parser:parser_interface",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
    ],
)

cc_test(
    name = "runtime_ext_registry_test",
    srcs = ["runtime_ext_registry_test.cc"],
    deps = [
        ":runtime_ext_registry",
        "//common:ast",
        "//common:source",
        "//common:value",
        "//common:value_testing",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:testing_descriptor_pool",
        "//parser",
        "//parser:options",
        "//parser:parser_interface",
        "//runtime",
        "//runtime:activation",
        "//runtime:function",
        "//runtime:function_adapter",
        "//runtime:function_registry",
        "//runtime:runtime_builder",
        "//runtime:runtime_builder_factory",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)
