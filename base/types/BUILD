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
    name = "any",
    srcs = ["any_type.cc"],
    hdrs = ["any_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "bool",
    srcs = ["bool_type.cc"],
    hdrs = ["bool_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "bytes",
    srcs = ["bytes_type.cc"],
    hdrs = ["bytes_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "double",
    srcs = ["double_type.cc"],
    hdrs = ["double_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "duration",
    srcs = ["duration_type.cc"],
    hdrs = ["duration_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "dyn",
    srcs = ["dyn_type.cc"],
    hdrs = ["dyn_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "enum",
    srcs = ["enum_type.cc"],
    hdrs = ["enum_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:rtti",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "error",
    srcs = ["error_type.cc"],
    hdrs = ["error_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "int",
    srcs = ["int_type.cc"],
    hdrs = ["int_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "list",
    srcs = ["list_type.cc"],
    hdrs = ["list_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "map",
    srcs = ["map_type.cc"],
    hdrs = ["map_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "null",
    srcs = ["null_type.cc"],
    hdrs = ["null_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "string",
    srcs = ["string_type.cc"],
    hdrs = ["string_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "struct",
    srcs = ["struct_type.cc"],
    hdrs = ["struct_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:rtti",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "timestamp",
    srcs = ["timestamp_type.cc"],
    hdrs = ["timestamp_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "type",
    srcs = ["type_type.cc"],
    hdrs = ["type_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "uint",
    srcs = ["uint_type.cc"],
    hdrs = ["uint_type.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)
