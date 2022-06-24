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

cc_library(
    name = "assume_aligned",
    hdrs = ["assume_aligned.h"],
    deps = [
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "unreachable",
    hdrs = ["unreachable.h"],
    deps = [
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "launder",
    hdrs = ["launder.h"],
    deps = [
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "benchmark",
    testonly = True,
    hdrs = ["benchmark.h"],
    deps = [
        "@com_github_google_benchmark//:benchmark_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "casts",
    hdrs = ["casts.h"],
)

cc_library(
    name = "status_builder",
    hdrs = ["status_builder.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "overflow",
    srcs = ["overflow.cc"],
    hdrs = ["overflow.h"],
    deps = [
        ":status_macros",
        ":time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "overflow_test",
    srcs = ["overflow_test.cc"],
    deps = [
        ":overflow",
        ":testing",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
    deps = [
        ":status_builder",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "strings",
    srcs = ["strings.cc"],
    hdrs = ["strings.h"],
    deps = [
        ":lexis",
        ":unicode",
        ":utf8",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "strings_test",
    srcs = ["strings_test.cc"],
    deps = [
        ":strings",
        ":testing",
        ":utf8",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "lexis",
    srcs = ["lexis.cc"],
    hdrs = ["lexis.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "lexis_test",
    srcs = ["lexis_test.cc"],
    deps = [
        ":lexis",
        ":testing",
    ],
)

cc_library(
    name = "no_destructor",
    hdrs = ["no_destructor.h"],
)

cc_library(
    name = "proto_util",
    srcs = ["proto_util.cc"],
    hdrs = ["proto_util.h"],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_util_test",
    srcs = ["proto_util_test.cc"],
    deps = [
        ":proto_util",
        ":testing",
        "//eval/public/structs:cel_proto_descriptor_pool_builder",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "proto_time_encoding",
    srcs = ["proto_time_encoding.cc"],
    hdrs = ["proto_time_encoding.h"],
    deps = [
        ":status_macros",
        ":time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_time_encoding_test",
    srcs = ["proto_time_encoding_test.cc"],
    deps = [
        ":proto_time_encoding",
        ":testing",
        "//testutil:util",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "rtti",
    hdrs = ["rtti.h"],
)

cc_test(
    name = "rtti_test",
    srcs = ["rtti_test.cc"],
    deps = [
        ":rtti",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
    ],
)

cc_library(
    name = "testing",
    testonly = True,
    srcs = [
        "testing.cc",
    ],
    hdrs = [
        "testing.h",
    ],
    deps = [
        ":status_builder",
        ":status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "time",
    srcs = ["time.cc"],
    hdrs = ["time.h"],
    deps = [
        ":status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "time_test",
    srcs = ["time_test.cc"],
    deps = [
        ":testing",
        ":time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "unicode",
    hdrs = ["unicode.h"],
)

cc_library(
    name = "utf8",
    srcs = ["utf8.cc"],
    hdrs = ["utf8.h"],
    deps = [
        ":unicode",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "utf8_test",
    srcs = ["utf8_test.cc"],
    deps = [
        ":benchmark",
        ":testing",
        ":utf8",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)
