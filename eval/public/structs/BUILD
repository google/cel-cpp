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

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "cel_proto_wrapper",
    srcs = [
        "cel_proto_wrapper.cc",
    ],
    hdrs = [
        "cel_proto_wrapper.h",
    ],
    deps = [
        ":cel_proto_wrap_util",
        ":proto_message_type_adapter",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//internal:proto_time_encoding",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "protobuf_value_factory",
    hdrs = [
        "protobuf_value_factory.h",
    ],
    deps = [
        "//eval/public:cel_value",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_proto_wrap_util",
    srcs = [
        "cel_proto_wrap_util.cc",
    ],
    hdrs = [
        "cel_proto_wrap_util.h",
    ],
    deps = [
        ":protobuf_value_factory",
        "//eval/public:cel_value",
        "//eval/testutil:test_message_cc_proto",
        "//internal:overflow",
        "//internal:proto_time_encoding",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_proto_wrap_util_test",
    size = "small",
    srcs = [
        "cel_proto_wrap_util_test.cc",
    ],
    deps = [
        ":cel_proto_wrap_util",
        ":protobuf_value_factory",
        ":trivial_legacy_type_info",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "//internal:no_destructor",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "field_access_impl",
    srcs = [
        "field_access_impl.cc",
    ],
    hdrs = [
        "field_access_impl.h",
    ],
    deps = [
        ":cel_proto_wrap_util",
        ":protobuf_value_factory",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//internal:casts",
        "//internal:overflow",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "field_access_impl_test",
    srcs = ["field_access_impl_test.cc"],
    deps = [
        ":cel_proto_wrapper",
        ":field_access_impl",
        "//eval/public:cel_value",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "//internal:time",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cel_proto_descriptor_pool_builder",
    srcs = ["cel_proto_descriptor_pool_builder.cc"],
    hdrs = ["cel_proto_descriptor_pool_builder.h"],
    deps = [
        "//internal:proto_util",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_proto_descriptor_pool_builder_test",
    srcs = ["cel_proto_descriptor_pool_builder_test.cc"],
    deps = [
        ":cel_proto_descriptor_pool_builder",
        "//eval/testutil:test_message_cc_proto",
        "//internal:testing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cel_proto_wrapper_test",
    size = "small",
    srcs = [
        "cel_proto_wrapper_test.cc",
    ],
    deps = [
        ":cel_proto_wrapper",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/testutil:test_message_cc_proto",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "legacy_type_provider",
    hdrs = ["legacy_type_provider.h"],
    deps = [
        ":legacy_type_adapter",
        "//base:type_provider",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "legacy_type_adapter",
    hdrs = ["legacy_type_adapter.h"],
    deps = [
        "//base:memory_manager",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "legacy_type_adapter_test",
    srcs = ["legacy_type_adapter_test.cc"],
    deps = [
        ":legacy_type_adapter",
        ":trivial_legacy_type_info",
        "//eval/public:cel_value",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "proto_message_type_adapter",
    srcs = ["proto_message_type_adapter.cc"],
    hdrs = ["proto_message_type_adapter.h"],
    deps = [
        ":cel_proto_wrap_util",
        ":field_access_impl",
        ":legacy_type_adapter",
        ":legacy_type_info_apis",
        "//base:memory_manager",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//eval/public/containers:internal_field_backed_list_impl",
        "//eval/public/containers:internal_field_backed_map_impl",
        "//extensions/protobuf:memory_manager",
        "//internal:casts",
        "//internal:no_destructor",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_message_type_adapter_test",
    srcs = ["proto_message_type_adapter_test.cc"],
    deps = [
        ":cel_proto_wrapper",
        ":legacy_type_adapter",
        ":legacy_type_info_apis",
        ":proto_message_type_adapter",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/containers:field_access",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//internal:testing",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "protobuf_descriptor_type_provider",
    srcs = ["protobuf_descriptor_type_provider.cc"],
    hdrs = ["protobuf_descriptor_type_provider.h"],
    deps = [
        ":legacy_type_provider",
        ":proto_message_type_adapter",
        "//eval/public:cel_value",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "protobuf_descriptor_type_provider_test",
    srcs = ["protobuf_descriptor_type_provider_test.cc"],
    deps = [
        ":protobuf_descriptor_type_provider",
        "//eval/public:cel_value",
        "//eval/public/testing:matchers",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//internal:testing",
    ],
)

cc_library(
    name = "legacy_type_info_apis",
    hdrs = ["legacy_type_info_apis.h"],
    deps = ["//eval/public:message_wrapper"],
)

cc_library(
    name = "trivial_legacy_type_info",
    testonly = True,
    hdrs = ["trivial_legacy_type_info.h"],
    deps = [
        ":legacy_type_info_apis",
        "//eval/public:message_wrapper",
        "//internal:no_destructor",
    ],
)

cc_test(
    name = "trivial_legacy_type_info_test",
    srcs = ["trivial_legacy_type_info_test.cc"],
    deps = [
        ":trivial_legacy_type_info",
        "//eval/public:message_wrapper",
        "//internal:testing",
    ],
)
