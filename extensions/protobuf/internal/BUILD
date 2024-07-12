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
    name = "any_lite",
    srcs = ["any_lite.cc"],
    hdrs = ["any_lite.h"],
    deps = [
        "//common:any",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "any",
    srcs = ["any.cc"],
    hdrs = ["any.h"],
    deps = [
        ":any_lite",
        ":is_generated_message",
        ":is_message_lite",
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
        ":any_lite",
        "//common:any",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings:cord",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ast",
    srcs = ["ast.cc"],
    hdrs = ["ast.h"],
    deps = [
        ":constant",
        "//common:ast",
        "//common:constant",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "ast_test",
    srcs = ["ast_test.cc"],
    deps = [
        ":ast",
        "//common:ast",
        "//internal:proto_matchers",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "constant",
    srcs = ["constant.cc"],
    hdrs = ["constant.h"],
    deps = [
        "//common:constant",
        "//internal:proto_time_encoding",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "duration_lite",
    srcs = ["duration_lite.cc"],
    hdrs = ["duration_lite.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "duration",
    srcs = ["duration.cc"],
    hdrs = ["duration.h"],
    deps = [
        ":duration_lite",
        ":is_generated_message",
        ":is_message_lite",
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
    name = "duration_test",
    srcs = ["duration_test.cc"],
    deps = [
        ":duration",
        ":duration_lite",
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
    name = "timestamp_lite",
    srcs = ["timestamp_lite.cc"],
    hdrs = ["timestamp_lite.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "timestamp",
    srcs = ["timestamp.cc"],
    hdrs = ["timestamp.h"],
    deps = [
        ":is_generated_message",
        ":is_message_lite",
        ":timestamp_lite",
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
        ":timestamp_lite",
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
    name = "message",
    srcs = ["message.cc"],
    hdrs = ["message.h"],
    deps = [
        ":any",
        ":duration",
        ":json",
        ":map_reflection",
        ":qualify",
        ":struct",
        ":timestamp",
        ":wrappers",
        "//base:attributes",
        "//base/internal:message_wrapper",
        "//common:any",
        "//common:casting",
        "//common:json",
        "//common:memory",
        "//common:native_type",
        "//common:type",
        "//common:value",
        "//common:value_kind",
        "//common/internal:reference_count",
        "//extensions/protobuf:json",
        "//extensions/protobuf:memory_manager",
        "//internal:align",
        "//internal:casts",
        "//internal:new",
        "//internal:status_macros",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "message_test",
    srcs = ["message_test.cc"],
    deps = [
        ":message",
        "//internal:testing",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
    ],
)

cc_library(
    name = "wrappers_lite",
    srcs = ["wrappers_lite.cc"],
    hdrs = ["wrappers_lite.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "wrappers",
    srcs = ["wrappers.cc"],
    hdrs = ["wrappers.h"],
    deps = [
        ":is_generated_message",
        ":is_message_lite",
        ":wrappers_lite",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:absl_check",
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
        ":wrappers_lite",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "field_mask_lite",
    srcs = ["field_mask_lite.cc"],
    hdrs = ["field_mask_lite.h"],
    deps = [
        "//common:json",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "field_mask",
    srcs = ["field_mask.cc"],
    hdrs = ["field_mask.h"],
    deps = [
        ":field_mask_lite",
        ":is_generated_message",
        ":is_message_lite",
        "//common:json",
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
        ":field_mask_lite",
        "//internal:testing",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    deps = [
        ":any",
        ":duration",
        ":field_mask",
        ":field_mask_lite",
        ":map_reflection",
        ":struct",
        ":struct_lite",
        ":timestamp",
        ":wrappers",
        "//common:any",
        "//common:json",
        "//common:value",
        "//internal:status_macros",
        "//internal:time",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "json_test",
    srcs = ["json_test.cc"],
    deps = [
        ":json",
        "//common:json",
        "//common:value_testing",
        "//internal:testing",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "struct_lite",
    srcs = ["struct_lite.cc"],
    hdrs = ["struct_lite.h"],
    deps = [
        "//common:json",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "struct",
    srcs = ["struct.cc"],
    hdrs = ["struct.h"],
    deps = [
        ":is_generated_message",
        ":is_message_lite",
        ":map_reflection",
        ":struct_lite",
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
        ":struct_lite",
        "//common:json",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "qualify",
    srcs = ["qualify.cc"],
    hdrs = ["qualify.h"],
    deps = [
        ":map_reflection",
        "//base:attributes",
        "//base:builtins",
        "//common:kind",
        "//common:memory",
        "//internal:status_macros",
        "//runtime:runtime_options",
        "//runtime/internal:errors",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "legacy_value",
    srcs = select({
        "//conditions:default": ["legacy_value_heavy.cc"],
    }),
    hdrs = ["legacy_value.h"],
    deps = [
        "//common:value",
        "//eval/public:message_wrapper",
        "//eval/public/structs:proto_message_type_adapter",
        "//extensions/protobuf:value",
        "//internal:status_macros",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "is_message_lite",
    hdrs = ["is_message_lite.h"],
    deps = ["@com_google_protobuf//:protobuf"],
)

cc_library(
    name = "is_generated_message",
    hdrs = ["is_generated_message.h"],
    deps = ["@com_google_protobuf//:protobuf"],
)
