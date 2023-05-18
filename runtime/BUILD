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

package(
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "activation_interface",
    hdrs = ["activation_interface.h"],
    deps = [
        ":function_overload_reference",
        "//base:attributes",
        "//base:value",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_overload_reference",
    hdrs = ["function_overload_reference.h"],
    deps = [
        "//base:function",
        "//base:function_descriptor",
    ],
)

cc_library(
    name = "function_provider",
    hdrs = ["function_provider.h"],
    deps = [
        ":activation_interface",
        ":function_overload_reference",
        "//base:function_descriptor",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "activation",
    srcs = ["activation.cc"],
    hdrs = ["activation.h"],
    deps = [
        ":activation_interface",
        ":function_overload_reference",
        "//base:attributes",
        "//base:function",
        "//base:function_descriptor",
        "//base:handle",
        "//base:value",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "activation_test",
    srcs = ["activation_test.cc"],
    deps = [
        ":activation",
        "//base:attributes",
        "//base:function",
        "//base:function_descriptor",
        "//base:handle",
        "//base:memory",
        "//base:type",
        "//base:value",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_registry",
    srcs = ["function_registry.cc"],
    hdrs = ["function_registry.h"],
    deps =
        [
            ":activation_interface",
            ":function_overload_reference",
            ":function_provider",
            "//base:function",
            "//base:function_descriptor",
            "//base:kind",
            "@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/container:node_hash_map",
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/status:statusor",
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/types:optional",
            "@com_google_absl//absl/types:span",
        ],
)

cc_test(
    name = "function_registry_test",
    srcs = ["function_registry_test.cc"],
    deps = [
        ":activation",
        ":function_overload_reference",
        ":function_provider",
        ":function_registry",
        "//base:function",
        "//base:function_adapter",
        "//base:function_descriptor",
        "//base:kind",
        "//base:value",
        "//internal:testing",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "runtime_options",
    hdrs = ["runtime_options.h"],
)
