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

exports_files(["LICENSE"])

cc_library(
    name = "message_wrapper",
    hdrs = [
        "message_wrapper.h",
    ],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "message_wrapper_test",
    srcs = [
        "message_wrapper_test.cc",
    ],
    deps = [
        ":message_wrapper",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/testutil:test_message_cc_proto",
        "//internal:casts",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_value_internal",
    hdrs = [
        "cel_value_internal.h",
    ],
    deps = [
        ":message_wrapper",
        "//internal:casts",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_value",
    srcs = [
        "cel_value.cc",
    ],
    hdrs = [
        "cel_value.h",
    ],
    deps = [
        ":cel_value_internal",
        ":message_wrapper",
        "//base:kind",
        "//base:memory_manager",
        "//eval/public/structs:legacy_type_info_apis",
        "//extensions/protobuf:memory_manager",
        "//internal:casts",
        "//internal:status_macros",
        "//internal:utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_attribute",
    srcs = [
        "cel_attribute.cc",
    ],
    hdrs = [
        "cel_attribute.h",
    ],
    deps = [
        ":cel_value",
        ":cel_value_internal",
        "//base:attributes",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "cel_value_producer",
    hdrs = [
        "cel_value_producer.h",
    ],
    deps = [
        ":cel_value",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "unknown_attribute_set",
    hdrs = [
        "unknown_attribute_set.h",
    ],
    deps = ["//base:attributes"],
)

cc_library(
    name = "activation",
    srcs = [
        "activation.cc",
    ],
    hdrs = [
        "activation.h",
    ],
    deps = [
        ":base_activation",
        ":cel_attribute",
        ":cel_function",
        ":cel_value",
        ":cel_value_producer",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "activation_bind_helper",
    srcs = [
        "activation_bind_helper.cc",
    ],
    hdrs = [
        "activation_bind_helper.h",
    ],
    deps = [
        ":activation",
        "//eval/public/containers:field_access",
        "//eval/public/containers:field_backed_list_impl",
        "//eval/public/containers:field_backed_map_impl",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "cel_function",
    srcs = [
        "cel_function.cc",
    ],
    hdrs = [
        "cel_function.h",
    ],
    deps = [
        ":cel_value",
        "//base:functions",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "cel_function_adapter_impl",
    hdrs = [
        "cel_function_adapter_impl.h",
    ],
    deps = [
        ":cel_function",
        ":cel_function_registry",
        ":cel_value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_function_adapter",
    hdrs = [
        "cel_function_adapter.h",
    ],
    deps = [
        ":cel_function",
        ":cel_function_adapter_impl",
        ":cel_function_registry",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "portable_cel_function_adapter",
    hdrs = [
        "portable_cel_function_adapter.h",
    ],
    deps = [
        ":cel_function",
        ":cel_function_adapter_impl",
        ":cel_function_registry",
        ":cel_value",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "portable_cel_function_adapter_test",
    size = "small",
    srcs = [
        "portable_cel_function_adapter_test.cc",
    ],
    deps = [
        ":portable_cel_function_adapter",
        "//internal:status_macros",
        "//internal:testing",
    ],
)

cc_library(
    name = "cel_function_provider",
    srcs = [
        "cel_function_provider.cc",
    ],
    hdrs = [
        "cel_function_provider.h",
    ],
    deps = [
        ":base_activation",
        ":cel_function",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "cel_builtins",
    hdrs = [
        "cel_builtins.h",
    ],
)

cc_library(
    name = "builtin_func_registrar",
    srcs = [
        "builtin_func_registrar.cc",
    ],
    hdrs = [
        "builtin_func_registrar.h",
    ],
    deps = [
        ":cel_builtins",
        ":cel_function",
        ":cel_function_registry",
        ":cel_number",
        ":cel_options",
        ":cel_value",
        ":comparison_functions",
        ":portable_cel_function_adapter",
        "//eval/eval:mutable_list_impl",
        "//eval/public/containers:container_backed_list_impl",
        "//internal:casts",
        "//internal:overflow",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "//internal:time",
        "//internal:utf8",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "comparison_functions",
    srcs = [
        "comparison_functions.cc",
    ],
    hdrs = [
        "comparison_functions.h",
    ],
    deps = [
        ":cel_builtins",
        ":cel_function_registry",
        ":cel_number",
        ":cel_options",
        ":cel_value",
        ":message_wrapper",
        ":portable_cel_function_adapter",
        "//eval/eval:mutable_list_impl",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:legacy_type_info_apis",
        "//internal:casts",
        "//internal:overflow",
        "//internal:status_macros",
        "//internal:time",
        "//internal:utf8",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "comparison_functions_test",
    size = "small",
    srcs = [
        "comparison_functions_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_builtins",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_function_registry",
        ":cel_options",
        ":cel_value",
        ":comparison_functions",
        ":message_wrapper",
        ":set_util",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/containers:field_backed_list_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googleapis//google/rpc/context:attribute_context_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "extension_func_registrar",
    srcs = [
        "extension_func_registrar.cc",
    ],
    hdrs = [
        "extension_func_registrar.h",
    ],
    deps = [
        ":cel_function",
        ":cel_function_adapter",
        ":cel_function_registry",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/type:timeofday_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_expression",
    hdrs = [
        "cel_expression.h",
    ],
    deps = [
        ":base_activation",
        ":cel_function",
        ":cel_function_registry",
        ":cel_type_registry",
        ":cel_value",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "source_position",
    srcs = ["source_position.cc"],
    hdrs = ["source_position.h"],
    deps = [
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "source_position_native",
    srcs = ["source_position_native.cc"],
    hdrs = ["source_position_native.h"],
    deps = [
        "//base:ast",
    ],
)

cc_library(
    name = "ast_visitor",
    hdrs = [
        "ast_visitor.h",
    ],
    deps = [
        ":source_position",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_visitor_base",
    hdrs = [
        "ast_visitor_base.h",
    ],
    deps = [
        ":ast_visitor",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_visitor_native",
    hdrs = [
        "ast_visitor_native.h",
    ],
    deps = [
        ":source_position_native",
        "//base:ast",
    ],
)

cc_library(
    name = "ast_visitor_native_base",
    hdrs = [
        "ast_visitor_native_base.h",
    ],
    deps = [
        ":ast_visitor_native",
    ],
)

cc_library(
    name = "ast_traverse",
    srcs = [
        "ast_traverse.cc",
    ],
    hdrs = [
        "ast_traverse.h",
    ],
    deps = [
        ":ast_visitor",
        ":source_position",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_traverse_native",
    srcs = [
        "ast_traverse_native.cc",
    ],
    hdrs = [
        "ast_traverse_native.h",
    ],
    deps = [
        ":ast_visitor_native",
        ":source_position_native",
        "//base:ast",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "cel_options",
    hdrs = [
        "cel_options.h",
    ],
    deps = [
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_expr_builder_factory",
    srcs = [
        "cel_expr_builder_factory.cc",
    ],
    hdrs = [
        "cel_expr_builder_factory.h",
    ],
    deps = [
        ":cel_expression",
        ":cel_options",
        ":portable_cel_expr_builder_factory",
        "//eval/compiler:flat_expr_builder",
        "//eval/public/structs:proto_message_type_adapter",
        "//eval/public/structs:protobuf_descriptor_type_provider",
        "//internal:proto_util",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "value_export_util",
    srcs = [
        "value_export_util.cc",
    ],
    hdrs = [
        "value_export_util.h",
    ],
    deps = [
        ":cel_value",
        "//internal:proto_time_encoding",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_function_registry",
    srcs = ["cel_function_registry.cc"],
    hdrs = ["cel_function_registry.h"],
    deps = [
        ":cel_function",
        ":cel_function_provider",
        ":cel_options",
        ":cel_value",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "cel_value_test",
    size = "small",
    srcs = [
        "cel_value_test.cc",
    ],
    deps = [
        ":cel_value",
        ":cel_value_internal",
        ":unknown_attribute_set",
        ":unknown_set",
        "//base:kind",
        "//base:memory_manager",
        "//eval/public/structs:legacy_type_info_apis",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "cel_attribute_test",
    size = "small",
    srcs = [
        "cel_attribute_test.cc",
    ],
    deps = [
        ":cel_attribute",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "activation_test",
    size = "small",
    srcs = [
        "activation_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_attribute",
        ":cel_function",
        "//eval/eval:attribute_trail",
        "//eval/eval:ident_step",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
    ],
)

cc_test(
    name = "ast_traverse_test",
    srcs = [
        "ast_traverse_test.cc",
    ],
    deps = [
        ":ast_traverse",
        ":ast_visitor",
        "//internal:testing",
    ],
)

cc_test(
    name = "ast_traverse_native_test",
    srcs = [
        "ast_traverse_native_test.cc",
    ],
    deps = [
        ":ast_traverse_native",
        ":ast_visitor_native",
        "//internal:testing",
    ],
)

cc_library(
    name = "ast_rewrite",
    srcs = [
        "ast_rewrite.cc",
    ],
    hdrs = [
        "ast_rewrite.h",
    ],
    deps = [
        ":ast_visitor",
        ":source_position",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "ast_rewrite_test",
    srcs = [
        "ast_rewrite_test.cc",
    ],
    deps = [
        ":ast_rewrite",
        ":ast_visitor",
        ":source_position",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "//testutil:util",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "ast_rewrite_native",
    srcs = [
        "ast_rewrite_native.cc",
    ],
    hdrs = [
        "ast_rewrite_native.h",
    ],
    deps = [
        ":ast_visitor_native",
        ":source_position_native",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "ast_rewrite_native_test",
    srcs = [
        "ast_rewrite_native_test.cc",
    ],
    deps = [
        ":ast_rewrite_native",
        ":ast_visitor_native",
        ":source_position_native",
        "//base:ast_utility",
        "//internal:testing",
        "//parser",
        "//testutil:util",
    ],
)

cc_test(
    name = "activation_bind_helper_test",
    size = "small",
    srcs = [
        "activation_bind_helper_test.cc",
    ],
    deps = [
        ":activation",
        ":activation_bind_helper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "cel_function_provider_test",
    srcs = [
        "cel_function_provider_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_function_provider",
        "//internal:status_macros",
        "//internal:testing",
    ],
)

cc_test(
    name = "cel_function_registry_test",
    srcs = [
        "cel_function_registry_test.cc",
    ],
    deps = [
        ":activation",
        ":cel_function",
        ":cel_function_provider",
        ":cel_function_registry",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "cel_function_adapter_test",
    size = "small",
    srcs = [
        "cel_function_adapter_test.cc",
    ],
    deps = [
        ":cel_function_adapter",
        "//internal:status_macros",
        "//internal:testing",
    ],
)

cc_library(
    name = "cel_type_registry",
    srcs = ["cel_type_registry.cc"],
    hdrs = ["cel_type_registry.h"],
    deps = [
        ":cel_value",
        "//eval/public/structs:legacy_type_provider",
        "//internal:no_destructor",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_type_registry_test",
    srcs = ["cel_type_registry_test.cc"],
    deps = [
        ":cel_type_registry",
        ":cel_value",
        "//eval/public/structs:legacy_type_provider",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "builtin_func_test",
    size = "small",
    srcs = [
        "builtin_func_test.cc",
    ],
    deps = [
        ":activation",
        ":builtin_func_registrar",
        ":cel_builtins",
        ":cel_expr_builder_factory",
        ":cel_function_registry",
        ":cel_options",
        ":cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "//internal:time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "extension_func_test",
    size = "small",
    srcs = [
        "extension_func_test.cc",
    ],
    deps = [
        ":builtin_func_registrar",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_function_registry",
        ":cel_value",
        ":extension_func_registrar",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/type:timeofday_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "source_position_test",
    size = "small",
    srcs = [
        "source_position_test.cc",
    ],
    deps = [
        ":source_position",
        "//internal:testing",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "source_position_native_test",
    size = "small",
    srcs = [
        "source_position_native_test.cc",
    ],
    deps = [
        ":source_position_native",
        "//internal:testing",
    ],
)

cc_test(
    name = "unknown_attribute_set_test",
    size = "small",
    srcs = [
        "unknown_attribute_set_test.cc",
    ],
    deps = [
        ":cel_attribute",
        ":cel_value",
        ":unknown_attribute_set",
        "//internal:testing",
    ],
)

cc_test(
    name = "value_export_util_test",
    size = "small",
    srcs = [
        "value_export_util_test.cc",
    ],
    deps = [
        ":value_export_util",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "unknown_function_result_set",
    srcs = ["unknown_function_result_set.cc"],
    hdrs = ["unknown_function_result_set.h"],
    deps = [
        ":cel_function",
        "//base:functions",
    ],
)

cc_test(
    name = "unknown_function_result_set_test",
    size = "small",
    srcs = [
        "unknown_function_result_set_test.cc",
    ],
    deps = [
        ":cel_function",
        ":cel_value",
        ":unknown_function_result_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:testing",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "unknown_set",
    hdrs = ["unknown_set.h"],
    deps = [
        ":unknown_attribute_set",
        ":unknown_function_result_set",
        "//base/internal:unknown_set",
    ],
)

cc_test(
    name = "unknown_set_test",
    srcs = ["unknown_set_test.cc"],
    deps = [
        ":cel_attribute",
        ":cel_function",
        ":unknown_attribute_set",
        ":unknown_function_result_set",
        ":unknown_set",
        "//internal:testing",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "transform_utility",
    srcs = [
        "transform_utility.cc",
    ],
    hdrs = [
        "transform_utility.h",
    ],
    deps = [
        ":cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:value_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "set_util",
    srcs = ["set_util.cc"],
    hdrs = ["set_util.h"],
    deps = [":cel_value"],
)

cc_library(
    name = "base_activation",
    hdrs = ["base_activation.h"],
    deps = [
        ":cel_attribute",
        ":cel_function",
        ":cel_value",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "set_util_test",
    size = "small",
    srcs = [
        "set_util_test.cc",
    ],
    deps = [
        ":cel_value",
        ":set_util",
        ":unknown_set",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "builtin_func_registrar_test",
    srcs = ["builtin_func_registrar_test.cc"],
    deps = [
        ":activation",
        ":builtin_func_registrar",
        ":cel_expr_builder_factory",
        ":cel_expression",
        ":cel_options",
        ":cel_value",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//internal:time",
        "//parser",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_number",
    srcs = ["cel_number.cc"],
    hdrs = ["cel_number.h"],
    deps = [
        ":cel_value",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "portable_cel_expr_builder_factory",
    srcs = ["portable_cel_expr_builder_factory.cc"],
    hdrs = ["portable_cel_expr_builder_factory.h"],
    deps = [
        ":cel_expression",
        ":cel_options",
        "//eval/compiler:flat_expr_builder",
        "//eval/public/structs:legacy_type_provider",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "portable_cel_expr_builder_factory_test",
    srcs = ["portable_cel_expr_builder_factory_test.cc"],
    deps = [
        ":activation",
        ":builtin_func_registrar",
        ":cel_options",
        ":cel_value",
        ":portable_cel_expr_builder_factory",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:legacy_type_info_apis",
        "//eval/public/structs:legacy_type_provider",
        "//eval/testutil:test_message_cc_proto",
        "//internal:casts",
        "//internal:proto_time_encoding",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_number_test",
    srcs = ["cel_number_test.cc"],
    deps = [
        ":cel_number",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
    ],
)
