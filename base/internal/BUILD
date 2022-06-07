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

# These headers should only ever be used by ../handle.h. They are here to avoid putting
# large amounts of implementation details in public headers.
cc_library(
    name = "handle",
    textual_hdrs = [
        "handle.pre.h",
        "handle.post.h",
    ],
    deps = [
        "//base:memory_manager",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "memory_manager",
    textual_hdrs = [
        "memory_manager.pre.h",
        "memory_manager.post.h",
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
        "type.pre.h",
        "type.post.h",
    ],
    deps = [
        "//base:handle",
        "//internal:rtti",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/numeric:bits",
    ],
)

cc_library(
    name = "value",
    textual_hdrs = [
        "value.pre.h",
        "value.post.h",
    ],
    deps = [
        "//base:handle",
        "//internal:casts",
        "//internal:rtti",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:variant",
    ],
)
