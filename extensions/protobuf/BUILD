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
        "//base:ast_internal",
        "//base/internal:ast_impl",
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
        "//base:ast_internal",
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
    name = "type",
    srcs = [
        "enum_type.cc",
        "struct_type.cc",
        "type.cc",
        "type_provider.cc",
    ],
    hdrs = [
        "enum_type.h",
        "struct_type.h",
        "type.h",
        "type_provider.h",
    ],
    deps = [
        "//base:data",
        "//base:handle",
        "//base:memory",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
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
        ":type",
        "//base:data",
        "//base:kind",
        "//base:memory",
        "//base/internal:memory_manager_testing",
        "//base/testing:type_matchers",
        "//extensions/protobuf/internal:testing",
        "//internal:testing",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value",
    srcs = [
        "enum_value.cc",
        "struct_value.cc",
        "value.cc",
    ],
    hdrs = [
        "enum_value.h",
        "struct_value.h",
        "value.h",
    ],
    deps = [
        ":memory_manager",
        ":type",
        "//base:handle",
        "//base:kind",
        "//base:memory",
        "//base:owner",
        "//base:type",
        "//base:value",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//eval/public:message_wrapper",
        "//eval/public/structs:proto_message_type_adapter",
        "//extensions/protobuf/internal:map_reflection",
        "//extensions/protobuf/internal:reflection",
        "//extensions/protobuf/internal:time",
        "//extensions/protobuf/internal:wrappers",
        "//internal:casts",
        "//internal:rtti",
        "//internal:status_macros",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "value_test",
    srcs = [
        "struct_value_test.cc",
        "value_test.cc",
    ],
    deps = [
        ":type",
        ":value",
        "//base:type",
        "//base:value",
        "//base/internal:memory_manager_testing",
        "//base/testing:value_matchers",
        "//extensions/protobuf/internal:descriptors",
        "//extensions/protobuf/internal:testing",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
