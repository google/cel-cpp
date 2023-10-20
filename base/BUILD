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
    name = "attributes",
    srcs = [
        "attribute.cc",
    ],
    hdrs = [
        "attribute.h",
        "attribute_set.h",
    ],
    deps = [
        ":kind",
        "//internal:status_macros",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "handle",
    hdrs = ["handle.h"],
    deps = [
        "//base/internal:data",
        "//base/internal:handle",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
    ],
)

cc_library(
    name = "owner",
    hdrs = ["owner.h"],
    deps = [
        "//base/internal:data",
    ],
)

cc_library(
    name = "kind",
    srcs = ["kind.cc"],
    hdrs = ["kind.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "kind_test",
    srcs = ["kind_test.cc"],
    deps = [
        ":kind",
        "//internal:testing",
    ],
)

cc_library(
    name = "memory",
    srcs = ["memory.cc"],
    hdrs = [
        "memory.h",
    ],
    deps = [
        ":handle",
        "//base/internal:data",
        "//base/internal:memory_manager",
        "//common:native_type",
        "//internal:no_destructor",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:dynamic_annotations",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "memory_test",
    srcs = [
        "memory_test.cc",
    ],
    deps = [
        ":memory",
        "//internal:testing",
    ],
)

cc_library(
    name = "operators",
    srcs = ["operators.cc"],
    hdrs = ["operators.h"],
    deps = [
        "//base/internal:operators",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "operators_test",
    srcs = ["operators_test.cc"],
    deps = [
        ":operators",
        "//base/internal:operators",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

# Build target encompassing cel::Type, cel::Value, and their related classes.
cc_library(
    name = "data",
    srcs = [
        "type.cc",
        "type_factory.cc",
        "type_manager.cc",
        "type_provider.cc",
        "value.cc",
        "value_factory.cc",
    ] + glob(
        [
            "types/*.cc",
            "values/*.cc",
        ],
        exclude = [
            "types/*_test.cc",
            "values/*_test.cc",
        ],
    ),
    hdrs = [
        "type.h",
        "type_factory.h",
        "type_manager.h",
        "type_provider.h",
        "type_registry.h",
        "value.h",
        "value_factory.h",
    ] + glob(
        [
            "types/*.h",
            "values/*.h",
        ],
    ),
    deps = [
        ":attributes",
        ":function_result_set",
        ":handle",
        ":kind",
        ":memory",
        ":owner",
        "//base/internal:data",
        "//base/internal:message_wrapper",
        "//base/internal:type",
        "//base/internal:unknown_set",
        "//base/internal:value",
        "//common:any",
        "//common:json",
        "//common:native_type",
        "//internal:casts",
        "//internal:no_destructor",
        "//internal:number",
        "//internal:overloaded",
        "//internal:proto_wire",
        "//internal:status_macros",
        "//internal:strings",
        "//internal:time",
        "//internal:utf8",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_absl//absl/utility",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "data_test",
    srcs = [
        "type_factory_test.cc",
        "type_provider_test.cc",
        "type_test.cc",
        "value_factory_test.cc",
        "value_test.cc",
    ] + glob([
        "types/*_test.cc",
        "values/*_test.cc",
    ]),
    deps = [
        ":data",
        ":handle",
        ":kind",
        ":memory",
        "//base/internal:data",
        "//base/internal:memory_manager_testing",
        "//base/testing:value_matchers",
        "//common:json",
        "//internal:benchmark",
        "//internal:status_macros",
        "//internal:strings",
        "//internal:testing",
        "//internal:time",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

alias(
    name = "type",
    actual = ":data",
    deprecation = "Use :data instead.",
)

alias(
    name = "value",
    actual = ":data",
    deprecation = "Use :data instead.",
)

cc_library(
    name = "function",
    hdrs = [
        "function.h",
    ],
    deps = [
        ":data",
        ":handle",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_descriptor",
    srcs = [
        "function_descriptor.cc",
    ],
    hdrs = [
        "function_descriptor.h",
    ],
    deps = [
        ":kind",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_result",
    hdrs = [
        "function_result.h",
    ],
    deps = [":function_descriptor"],
)

cc_library(
    name = "function_result_set",
    srcs = [
        "function_result_set.cc",
    ],
    hdrs = [
        "function_result_set.h",
    ],
    deps = [
        ":function_result",
        "@com_google_absl//absl/container:btree",
    ],
)

cc_library(
    name = "ast",
    hdrs = ["ast.h"],
)

cc_library(
    name = "function_adapter",
    hdrs = ["function_adapter.h"],
    deps = [
        ":data",
        ":function",
        ":function_descriptor",
        ":handle",
        ":kind",
        "//base/internal:function_adapter",
        "//internal:status_macros",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "function_adapter_test",
    srcs = ["function_adapter_test.cc"],
    deps = [
        ":data",
        ":function",
        ":function_adapter",
        ":function_descriptor",
        ":handle",
        ":kind",
        ":memory",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "builtins",
    hdrs = ["builtins.h"],
)
