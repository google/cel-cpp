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
    name = "any",
    srcs = ["any.cc"],
    hdrs = ["any.h"],
    deps = [
        "//common:any",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "any_test",
    srcs = ["any_test.cc"],
    deps = [
        ":any",
        "//common:any",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings:cord",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "duration",
    srcs = ["duration.cc"],
    hdrs = ["duration.h"],
    deps = [
        "//internal:casts",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "duration_test",
    srcs = ["duration_test.cc"],
    deps = [
        ":duration",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "enum",
    hdrs = ["enum.h"],
    deps = ["@com_google_protobuf//:protobuf"],
)

cc_library(
    name = "timestamp",
    srcs = ["timestamp.cc"],
    hdrs = ["timestamp.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "timestamp_test",
    srcs = ["timestamp_test.cc"],
    deps = [
        ":timestamp",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "map_reflection",
    srcs = ["map_reflection.cc"],
    hdrs = ["map_reflection.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "reflection",
    srcs = ["reflection.cc"],
    hdrs = ["reflection.h"],
    deps = [
        "//common:value",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "wrappers",
    srcs = ["wrappers.cc"],
    hdrs = ["wrappers.h"],
    deps = [
        "//internal:casts",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "wrappers_test",
    srcs = ["wrappers_test.cc"],
    deps = [
        ":wrappers",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "field_mask",
    srcs = ["field_mask.cc"],
    hdrs = ["field_mask.h"],
    deps = [
        "//common:json",
        "//internal:casts",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "field_mask_test",
    srcs = ["field_mask_test.cc"],
    deps = [
        ":field_mask",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "struct",
    srcs = ["struct.cc"],
    hdrs = ["struct.h"],
    deps = [
        ":map_reflection",
        "//common:json",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "struct_test",
    srcs = ["struct_test.cc"],
    deps = [
        ":struct",
        "//common:json",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
    ],
)
