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

cc_library(
    name = "data",
    hdrs = ["data.h"],
    deps = [
        "//base:kind",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:bits",
    ],
)

# These headers should only ever be used by ../handle.h. They are here to avoid putting
# large amounts of implementation details in public headers.
cc_library(
    name = "handle",
    hdrs = ["handle.h"],
    deps = [
        ":data",
    ],
)

cc_library(
    name = "memory_manager",
    hdrs = [
        "memory_manager.h",
    ],
)

cc_library(
    name = "memory_manager_testing",
    testonly = True,
    srcs = ["memory_manager_testing.cc"],
    hdrs = ["memory_manager_testing.h"],
    deps = [
        "//internal:testing",
    ],
)

cc_library(
    name = "message_wrapper",
    hdrs = ["message_wrapper.h"],
)

cc_library(
    name = "operators",
    hdrs = ["operators.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

# These headers should only ever be used by ../type.h. They are here to avoid putting
# large amounts of implementation details in public headers.
cc_library(
    name = "type",
    textual_hdrs = [
        "type.h",
    ],
    deps = [
        ":data",
        "//base:kind",
        "//common:native_type",
    ],
)

cc_library(
    name = "unknown_set",
    srcs = ["unknown_set.cc"],
    hdrs = ["unknown_set.h"],
    deps = [
        "//base:attributes",
        "//base:function_result_set",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
    ],
)

cc_library(
    name = "value",
    textual_hdrs = [
        "value.h",
    ],
    deps = [
        ":data",
        ":type",
        "//base:handle",
        "//common:native_type",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "function_adapter",
    hdrs = [
        "function_adapter.h",
    ],
    deps = [
        "//base:data",
        "//base:handle",
        "//base:kind",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "function_adapter_test",
    srcs = ["function_adapter_test.cc"],
    deps = [
        ":function_adapter",
        "//base:data",
        "//base:handle",
        "//base:kind",
        "//base:memory",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)
