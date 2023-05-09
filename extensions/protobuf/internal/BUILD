# Copyright 2022 Google LLC
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
    name = "map_reflection",
    srcs = ["map_reflection.cc"],
    hdrs = ["map_reflection.h"],
    deps = ["@com_google_protobuf//:protobuf"],
)

cc_library(
    name = "testing",
    testonly = True,
    hdrs = ["testing.h"],
    deps = [
        "//base:memory",
        "//base/internal:memory_manager_testing",
        "//extensions/protobuf:memory_manager",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "time",
    srcs = ["time.cc"],
    hdrs = ["time.h"],
    deps = [
        "//internal:casts",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "time_test",
    srcs = ["time_test.cc"],
    deps = [
        ":time",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "wrappers",
    srcs = ["wrappers.cc"],
    hdrs = ["wrappers.h"],
    deps = [
        "//internal:casts",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "wrappers_test",
    srcs = ["wrappers_test.cc"],
    deps = [
        ":wrappers",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)
