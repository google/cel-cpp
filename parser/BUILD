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
    name = "parser",
    srcs = [
        "parser.cc",
    ],
    hdrs = [
        "parser.h",
    ],
    copts = [
        "-fexceptions",
    ],
    deps = [
        ":macro",
        ":macro_registry",
        ":options",
        ":source_factory",
        "//common:operators",
        "//common:source",
        "//internal:status_macros",
        "//internal:strings",
        "//parser/internal:cel_cc_parser",
        "@antlr4_runtimes//:cpp",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "macro",
    srcs = [
        "macro.cc",
    ],
    hdrs = [
        "macro.h",
    ],
    deps = [
        ":source_factory",
        "//common:operators",
        "//internal:lexis",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "macro_registry",
    srcs = [
        "macro_registry.cc",
    ],
    hdrs = [
        "macro_registry.h",
    ],
    deps = [
        ":macro",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "macro_registry_test",
    srcs = ["macro_registry_test.cc"],
    deps = [
        ":macro",
        ":macro_registry",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "source_factory",
    srcs = [
        "source_factory.cc",
    ],
    hdrs = [
        "source_factory.h",
    ],
    copts = [
        "-fexceptions",
    ],
    deps = [
        "//common:operators",
        "//common:source",
        "//parser/internal:cel_cc_parser",
        "@antlr4_runtimes//:cpp",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "options",
    hdrs = ["options.h"],
    deps = [
        "//parser/internal:options",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_test(
    name = "parser_test",
    srcs = ["parser_test.cc"],
    tags = ["benchmark"],
    deps = [
        ":macro",
        ":options",
        ":parser",
        ":source_factory",
        "//internal:benchmark",
        "//internal:testing",
        "//testutil:expr_printer",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)
