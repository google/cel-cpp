# Copyright 2023 Google LLC
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
    name = "composed_type_provider",
    srcs = ["composed_type_provider.cc"],
    hdrs = ["composed_type_provider.h"],
    deps = [
        "//base:data",
        "//common:type",
        "//internal:status_macros",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "mutable_list_impl",
    srcs = ["mutable_list_impl.cc"],
    hdrs = ["mutable_list_impl.h"],
    deps = [
        "//common:memory",
        "//common:native_type",
        "//common:type",
        "//common:value",
        "//internal:casts",
    ],
)

cc_test(
    name = "mutable_list_impl_test",
    srcs = ["mutable_list_impl_test.cc"],
    deps = [
        ":mutable_list_impl",
        "//base:data",
        "//common:memory",
        "//common:type",
        "//common:value",
        "//internal:testing",
    ],
)

cc_library(
    name = "runtime_friend_access",
    hdrs = ["runtime_friend_access.h"],
    deps = [
        "//common:native_type",
        "//runtime",
        "//runtime:runtime_builder",
    ],
)

cc_library(
    name = "runtime_impl",
    srcs = ["runtime_impl.cc"],
    hdrs = ["runtime_impl.h"],
    deps = [
        "//base:ast",
        "//base:data",
        "//common:native_type",
        "//common:value",
        "//eval/compiler:flat_expr_builder",
        "//eval/eval:evaluator_core",
        "//internal:status_macros",
        "//runtime",
        "//runtime:activation_interface",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "//runtime:type_registry",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "convert_constant",
    srcs = ["convert_constant.cc"],
    hdrs = ["convert_constant.h"],
    deps = [
        "//base:data",
        "//base/ast_internal:expr",
        "//common:value",
        "//eval/internal:errors",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "errors",
    srcs = ["errors.cc"],
    hdrs = ["errors.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "issue_collector",
    hdrs = ["issue_collector.h"],
    deps = [
        "//runtime:runtime_issue",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "issue_collector_test",
    srcs = ["issue_collector_test.cc"],
    deps = [
        ":issue_collector",
        "//internal:testing",
        "//runtime:runtime_issue",
        "@com_google_absl//absl/status",
    ],
)
