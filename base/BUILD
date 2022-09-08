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
    ] + glob(["types/*.cc"]),
    hdrs = [
        "type.h",
    ] + glob(["types/*.h"]),
    deps = [
        ":handle",
        ":kind",
        "//base/internal:data",
        "//base/internal:type",
        "//internal:casts",
        "//internal:rtti",
        "//internal:unreachable",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
        "@com_google_absl//absl/utility",
    ],
)

cc_library(
    name = "type_manager",
    srcs = ["type_manager.cc"],
    hdrs = ["type_manager.h"],
    deps = [
        ":type",
        ":type_factory",
        ":type_provider",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "type_provider",
    srcs = ["type_provider.cc"],
    hdrs = ["type_provider.h"],
    deps = [
        ":handle",
        ":type",
        ":type_factory",
        "//internal:no_destructor",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "type_registry",
    hdrs = ["type_registry.h"],
    deps = [":type_provider"],
)

cc_library(
    name = "type_factory",
    srcs = ["type_factory.cc"],
    hdrs = ["type_factory.h"],
    deps = [
        ":handle",
        ":memory_manager",
        ":type",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
    ],
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
        ":type_factory",
        ":type_manager",
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
    ] + glob(["values/*.cc"]),
    hdrs = [
        "value.h",
    ] + glob(["values/*.h"]),
    deps = [
        ":attributes",
        ":functions",
        ":handle",
        ":kind",
        ":type",
        "//base/internal:data",
        "//base/internal:unknown_set",
        "//base/internal:value",
        "//internal:casts",
        "//internal:rtti",
        "//internal:strings",
        "//internal:time",
        "//internal:unreachable",
        "//internal:utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "value_factory",
    srcs = ["value_factory.cc"],
    hdrs = ["value_factory.h"],
    deps = [
        ":attributes",
        ":functions",
        ":handle",
        ":memory_manager",
        ":type_manager",
        ":value",
        "//internal:status_macros",
        "//internal:time",
        "//internal:utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
    ],
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
        ":type_factory",
        ":type_manager",
        ":value",
        ":value_factory",
        "//base/internal:memory_manager_testing",
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
    name = "ast",
    srcs = ["ast.cc"],
    hdrs = [
        "ast.h",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "ast_test",
    srcs = [
        "ast_test.cc",
    ],
    deps = [
        ":ast",
        "//internal:testing",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "ast_utility",
    srcs = ["ast_utility.cc"],
    hdrs = ["ast_utility.h"],
    deps = [
        ":ast",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
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

cc_test(
    name = "ast_utility_test",
    srcs = [
        "ast_utility_test.cc",
    ],
    deps = [
        ":ast",
        ":ast_utility",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
