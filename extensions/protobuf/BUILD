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
    name = "memory_manager",
    srcs = ["memory_manager.cc"],
    hdrs = ["memory_manager.h"],
    deps = [
        "//base:memory",
        "//internal:casts",
        "//internal:rtti",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "memory_manager_test",
    srcs = ["memory_manager_test.cc"],
    deps = [
        ":memory_manager",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ast_converters",
    srcs = ["ast_converters.cc"],
    hdrs = ["ast_converters.h"],
    deps = [
        "//base:ast",
        "//base/ast_internal:ast_impl",
        "//base/ast_internal:expr",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "ast_converters_test",
    srcs = [
        "ast_converters_test.cc",
    ],
    deps = [
        ":ast_converters",
        "//base/ast_internal:expr",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "data",
    srcs = [
        "enum_type.cc",
        "enum_value.cc",
        "struct_type.cc",
        "struct_value.cc",
        "type.cc",
        "type_provider.cc",
        "value.cc",
    ],
    hdrs = [
        "enum_type.h",
        "enum_value.h",
        "struct_type.h",
        "struct_value.h",
        "type.h",
        "type_provider.h",
        "value.h",
    ],
    deps = [
        ":memory_manager",
        "//base:data",
        "//base:handle",
        "//base:kind",
        "//base:memory",
        "//base:owner",
        "//common:any",
        "//common:json",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//eval/public:message_wrapper",
        "//eval/public/structs:proto_message_type_adapter",
        "//extensions/protobuf/internal:any",
        "//extensions/protobuf/internal:duration",
        "//extensions/protobuf/internal:field_mask",
        "//extensions/protobuf/internal:map_reflection",
        "//extensions/protobuf/internal:reflection",
        "//extensions/protobuf/internal:struct",
        "//extensions/protobuf/internal:timestamp",
        "//extensions/protobuf/internal:wrappers",
        "//internal:casts",
        "//internal:rtti",
        "//internal:status_macros",
        "//internal:time",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

alias(
    name = "type",
    actual = ":data",
    deprecation = "Use :data instead.",
)

cc_test(
    name = "type_test",
    srcs = [
        "enum_type_test.cc",
        "struct_type_test.cc",
        "type_provider_test.cc",
        "type_test.cc",
    ],
    deps = [
        ":data",
        "//base:data",
        "//base:kind",
        "//base:memory",
        "//base/internal:memory_manager_testing",
        "//base/testing:type_matchers",
        "//extensions/protobuf/internal:testing",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

alias(
    name = "value",
    actual = ":data",
    deprecation = "Use :data instead.",
)

cc_test(
    name = "value_test",
    srcs = [
        "struct_value_test.cc",
        "value_test.cc",
    ],
    deps = [
        ":data",
        "//base:data",
        "//base/internal:memory_manager_testing",
        "//base/testing:value_matchers",
        "//common:json",
        "//extensions/protobuf/internal:descriptors",
        "//extensions/protobuf/internal:testing",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
