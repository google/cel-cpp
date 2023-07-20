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
    name = "number",
    hdrs = ["number.h"],
    deps = [
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "number_test",
    srcs = ["number_test.cc"],
    deps = [
        ":number",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "composed_type_provider",
    srcs = ["composed_type_provider.cc"],
    hdrs = ["composed_type_provider.h"],
    deps = [
        "//base:handle",
        "//base:type",
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
        "//base:data",
        "//base:handle",
        "//base:memory",
        "//internal:rtti",
    ],
)

cc_test(
    name = "mutable_list_impl_test",
    srcs = ["mutable_list_impl_test.cc"],
    deps = [
        ":mutable_list_impl",
        "//base:data",
        "//base:memory",
        "//internal:testing",
    ],
)
