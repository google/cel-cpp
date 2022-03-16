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
        "//eval/public:cel_value",
        "//eval/testutil:test_message_cc_proto",
        "//internal:overflow",
        "//internal:proto_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
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
        "//internal:proto_util",
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
    name = "legacy_type_adapter",
    hdrs = ["legacy_type_adapter.h"],
    deps = [
        "//base:memory_manager",
        "//eval/public:cel_value",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "legacy_type_adapter_test",
    srcs = ["legacy_type_adapter_test.cc"],
    deps = [
        ":legacy_type_adapter",
        "//eval/public:cel_value",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_protobuf//:protobuf",
    ],
)
