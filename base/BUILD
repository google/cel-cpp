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
    name = "allocator",
    hdrs = ["allocator.h"],
    deps = [
        ":memory_manager",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_test(
    name = "allocator_test",
    srcs = ["allocator_test.cc"],
    deps = [
        ":allocator",
        ":memory_manager",
        "//internal:testing",
    ],
)

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
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "handle",
    hdrs = ["handle.h"],
    deps = [
        ":memory_manager",
        "//base/internal:data",
        "//base/internal:handle",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/utility",
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
    name = "managed_memory",
    hdrs = ["managed_memory.h"],
    deps = ["//base/internal:managed_memory"],
)

cc_library(
    name = "memory_manager",
    srcs = ["memory_manager.cc"],
    hdrs = ["memory_manager.h"],
    deps = [
        ":managed_memory",
        "//base/internal:data",
        "//base/internal:memory_manager",
        "//internal:no_destructor",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:dynamic_annotations",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "memory_manager_test",
    srcs = ["memory_manager_test.cc"],
    deps = [
        ":memory_manager",
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
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "operators_test",
    srcs = ["operators_test.cc"],
    deps = [
        ":operators",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "type",
    srcs = [
        "type.cc",
        "type_factory.cc",
        "type_manager.cc",
        "type_provider.cc",
    ] + glob(["types/*.cc"]),
    hdrs = [
        "type.h",
        "type_factory.h",
        "type_manager.h",
        "type_provider.h",
        "type_registry.h",
    ] + glob(["types/*.h"]),
    deps = [
        ":handle",
        ":kind",
        ":memory_manager",
        "//base/internal:data",
        "//base/internal:message_wrapper",
        "//base/internal:type",
        "//internal:casts",
        "//internal:no_destructor",
        "//internal:rtti",
        "//internal:status_macros",
        "//internal:unreachable",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_absl//absl/utility",
    ],
)

alias(
    name = "type_manager",
    actual = "type",
    deprecation = "Use :type instead",
)

alias(
    name = "type_provider",
    actual = "type",
    deprecation = "Use :type instead",
)

alias(
    name = "type_registry",
    actual = "type",
    deprecation = "Use :type instead",
)

alias(
    name = "type_factory",
    actual = "type",
    deprecation = "Use :type instead",
)

cc_test(
    name = "type_test",
    srcs = [
        "type_factory_test.cc",
        "type_test.cc",
    ],
    deps = [
        ":handle",
        ":memory_manager",
        ":type",
        ":value",
        "//base/internal:memory_manager_testing",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "value",
    srcs = [
        "value.cc",
        "value_factory.cc",
    ] + glob(["values/*.cc"]),
    hdrs = [
        "value.h",
        "value_factory.h",
    ] + glob(["values/*.h"]),
    deps = [
        ":attributes",
        ":functions",
        ":handle",
        ":kind",
        ":memory_manager",
        ":type",
        "//base/internal:data",
        "//base/internal:message_wrapper",
        "//base/internal:unknown_set",
        "//base/internal:value",
        "//internal:casts",
        "//internal:rtti",
        "//internal:status_macros",
        "//internal:strings",
        "//internal:time",
        "//internal:unreachable",
        "//internal:utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_googlesource_code_re2//:re2",
    ],
)

alias(
    name = "value_factory",
    actual = "value",
    deprecation = "Use :value instead",
)

cc_test(
    name = "value_test",
    srcs = [
        "value_factory_test.cc",
        "value_test.cc",
    ],
    deps = [
        ":memory_manager",
        ":type",
        ":value",
        "//base/internal:memory_manager_testing",
        "//internal:benchmark",
        "//internal:strings",
        "//internal:testing",
        "//internal:time",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "ast_internal",
    srcs = ["ast_internal.cc"],
    hdrs = [
        "ast_internal.h",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "ast_internal_test",
    srcs = [
        "ast_internal_test.cc",
    ],
    deps = [
        ":ast_internal",
        "//internal:testing",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "functions",
    srcs = [
        "function.cc",
        "function_result_set.cc",
    ],
    hdrs = [
        "function.h",
        "function_result.h",
        "function_result_set.h",
    ],
    deps = [
        ":kind",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "ast",
    hdrs = ["ast.h"],
)

cc_library(
    name = "function_interface",
    hdrs = ["function_interface.h"],
    deps = [
        ":value",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_adapter",
    hdrs = ["function_adapter.h"],
    deps = [
        ":function_interface",
        ":functions",
        "//base/internal:function_adapter",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "function_adapter_test",
    srcs = ["function_adapter_test.cc"],
    deps = [
        ":function_adapter",
        ":function_interface",
        ":functions",
        ":handle",
        ":kind",
        ":memory_manager",
        ":type",
        ":value",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)
