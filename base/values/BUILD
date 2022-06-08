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
    name = "bool",
    srcs = ["bool_value.cc"],
    hdrs = ["bool_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:bool",
        "//internal:casts",
        "@com_google_absl//absl/hash",
    ],
)

cc_library(
    name = "bytes",
    srcs = ["bytes_value.cc"],
    hdrs = ["bytes_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:bytes",
        "//internal:casts",
        "//internal:strings",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "double",
    srcs = ["double_value.cc"],
    hdrs = ["double_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:double",
        "//internal:casts",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "duration",
    srcs = ["duration_value.cc"],
    hdrs = ["duration_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:duration",
        "//internal:casts",
        "//internal:time",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "enum",
    srcs = ["enum_value.cc"],
    hdrs = ["enum_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:enum",
        "//internal:casts",
        "//internal:rtti",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "error",
    srcs = ["error_value.cc"],
    hdrs = ["error_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:error",
        "//internal:casts",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "int",
    srcs = ["int_value.cc"],
    hdrs = ["int_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:int",
        "//internal:casts",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "list",
    srcs = ["list_value.cc"],
    hdrs = ["list_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:list",
        "//internal:rtti",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "map",
    srcs = ["map_value.cc"],
    hdrs = ["map_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:map",
        "//internal:rtti",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "null",
    srcs = ["null_value.cc"],
    hdrs = ["null_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:null",
        "//internal:no_destructor",
        "@com_google_absl//absl/hash",
    ],
)

cc_library(
    name = "string",
    srcs = ["string_value.cc"],
    hdrs = ["string_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:string",
        "//internal:casts",
        "//internal:strings",
        "//internal:utf8",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "struct",
    srcs = ["struct_value.cc"],
    hdrs = ["struct_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:struct",
        "//internal:casts",
        "//internal:rtti",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "timestamp",
    srcs = ["timestamp_value.cc"],
    hdrs = ["timestamp_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:timestamp",
        "//internal:casts",
        "//internal:time",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "type",
    srcs = ["type_value.cc"],
    hdrs = ["type_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:type",
        "//internal:casts",
        "@com_google_absl//absl/hash",
    ],
)

cc_library(
    name = "uint",
    srcs = ["uint_value.cc"],
    hdrs = ["uint_value.h"],
    deps = [
        "//base:kind",
        "//base:type",
        "//base:value",
        "//base/types:uint",
        "//internal:casts",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
    ],
)
