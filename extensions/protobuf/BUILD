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
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    deps = [
        "//common:any",
        "//common:json",
        "//extensions/protobuf/internal:json",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "memory_manager",
    srcs = ["memory_manager.cc"],
    hdrs = ["memory_manager.h"],
    deps = [
        "//common:memory",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "memory_manager_test",
    srcs = ["memory_manager_test.cc"],
    deps = [
        ":memory_manager",
        "//common:memory",
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
        "//common:constant",
        "//extensions/protobuf/internal:ast",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:overload",
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
        "//base/ast_internal:ast_impl",
        "//base/ast_internal:expr",
        "//internal:proto_matchers",
        "//internal:testing",
        "//parser",
        "//parser:options",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "runtime_adapter",
    srcs = ["runtime_adapter.cc"],
    hdrs = ["runtime_adapter.h"],
    deps = [
        ":ast_converters",
        "//internal:status_macros",
        "//runtime",
        "//runtime:runtime_builder",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "enum_adapter",
    srcs = ["enum_adapter.cc"],
    hdrs = ["enum_adapter.h"],
    deps = [
        "//runtime:type_registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "type",
    srcs = [
        "type.cc",
        "type_introspector.cc",
    ],
    hdrs = [
        "type.h",
        "type_introspector.h",
    ],
    deps = [
        "//common:type",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "type_test",
    srcs = [
        "type_introspector_test.cc",
        "type_test.cc",
    ],
    deps = [
        ":type",
        "//common:type",
        "//common:type_kind",
        "//common:type_testing",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value",
    srcs = [
        "type_reflector.cc",
        "value.cc",
    ],
    hdrs = [
        "type_reflector.h",
        "value.h",
    ],
    deps = [
        ":json",
        ":memory_manager",
        ":type",
        "//common:any",
        "//common:casting",
        "//common:json",
        "//common:memory",
        "//common:type",
        "//common:value",
        "//extensions/protobuf/internal:any",
        "//extensions/protobuf/internal:duration",
        "//extensions/protobuf/internal:duration_lite",
        "//extensions/protobuf/internal:enum",
        "//extensions/protobuf/internal:map_reflection",
        "//extensions/protobuf/internal:message",
        "//extensions/protobuf/internal:struct",
        "//extensions/protobuf/internal:struct_lite",
        "//extensions/protobuf/internal:timestamp",
        "//extensions/protobuf/internal:timestamp_lite",
        "//extensions/protobuf/internal:wrappers",
        "//extensions/protobuf/internal:wrappers_lite",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "value_test",
    srcs = [
        "type_reflector_test.cc",
        "value_test.cc",
    ],
    deps = [
        ":memory_manager",
        ":value",
        "//base:attributes",
        "//base/internal:message_wrapper",
        "//common:casting",
        "//common:memory",
        "//common:type",
        "//common:value",
        "//common:value_kind",
        "//common:value_testing",
        "//eval/public/structs:proto_message_type_adapter",
        "//internal:proto_matchers",
        "//internal:testing",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "value_end_to_end_test",
    srcs = ["value_end_to_end_test.cc"],
    deps = [
        ":runtime_adapter",
        ":value",
        "//common:memory",
        "//common:value",
        "//common:value_testing",
        "//internal:testing",
        "//parser",
        "//runtime",
        "//runtime:activation",
        "//runtime:runtime_options",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "bind_proto_to_activation",
    srcs = ["bind_proto_to_activation.cc"],
    hdrs = ["bind_proto_to_activation.h"],
    deps = [
        ":value",
        "//common:casting",
        "//common:value",
        "//extensions/protobuf/internal:message",
        "//internal:status_macros",
        "//runtime:activation",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "bind_proto_to_activation_test",
    srcs = ["bind_proto_to_activation_test.cc"],
    deps = [
        ":bind_proto_to_activation",
        ":value",
        "//common:casting",
        "//common:memory",
        "//common:value",
        "//common:value_testing",
        "//internal:testing",
        "//runtime:activation",
        "//runtime:managed_value_factory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value_testing",
    testonly = True,
    hdrs = ["value_testing.h"],
    deps = [
        ":value",
        "//common:value",
        "//extensions/protobuf/internal:message",
        "//internal:testing",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "value_testing_test",
    srcs = ["value_testing_test.cc"],
    deps = [
        ":memory_manager",
        ":value",
        ":value_testing",
        "//common:memory",
        "//common:value",
        "//common:value_testing",
        "//internal:proto_matchers",
        "//internal:testing",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
