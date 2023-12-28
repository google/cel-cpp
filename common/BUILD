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
    name = "operators",
    srcs = [
        "operators.cc",
    ],
    hdrs = [
        "operators.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "any",
    srcs = ["any.cc"],
    hdrs = ["any.h"],
    deps = [
        "//internal:strings",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "any_test",
    srcs = ["any_test.cc"],
    deps = [
        ":any",
        "//internal:testing",
    ],
)

cc_library(
    name = "casting",
    hdrs = ["casting.h"],
    deps = [
        "//common/internal:casting",
        "//internal:casts",
        "@com_google_absl//absl/meta:type_traits",
    ],
)

cc_test(
    name = "casting_test",
    srcs = ["casting_test.cc"],
    deps = [
        ":casting",
        ":native_type",
        "//internal:testing",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    deps = [
        ":any",
        "//internal:copy_on_write",
        "//internal:no_destructor",
        "//internal:overloaded",
        "//internal:proto_wire",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "json_test",
    srcs = ["json_test.cc"],
    deps = [
        ":json",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/strings",
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
        ":type_kind",
        ":value_kind",
        "//internal:testing",
    ],
)

cc_library(
    name = "memory",
    srcs = ["memory.cc"],
    hdrs = ["memory.h"],
    deps = [
        ":casting",
        ":native_type",
        "//common/internal:reference_count",
        "//internal:exceptions",
        "//internal:no_destructor",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/numeric:bits",
    ],
)

cc_test(
    name = "memory_test",
    srcs = ["memory_test.cc"],
    deps = [
        ":casting",
        ":memory",
        ":native_type",
        "//internal:testing",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "memory_testing",
    testonly = True,
    hdrs = ["memory_testing.h"],
    deps = [
        ":memory",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "type_testing",
    testonly = True,
    hdrs = ["type_testing.h"],
    deps = [
        ":memory",
        ":memory_testing",
        ":type",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "value_testing",
    testonly = True,
    hdrs = ["value_testing.h"],
    deps = [
        ":memory",
        ":memory_testing",
        ":type",
        ":value",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "type_kind",
    hdrs = ["type_kind.h"],
    deps = [
        ":kind",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "value_kind",
    hdrs = ["value_kind.h"],
    deps = [
        ":kind",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "source",
    srcs = ["source.cc"],
    hdrs = ["source.h"],
    deps = [
        "//internal:overloaded",
        "//internal:unicode",
        "//internal:utf8",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "source_test",
    srcs = ["source_test.cc"],
    deps = [
        ":source",
        "//internal:testing",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "native_type",
    srcs = ["native_type.cc"],
    hdrs = ["native_type.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "native_type_test",
    srcs = ["native_type_test.cc"],
    deps = [
        ":native_type",
        "//internal:testing",
        "@com_google_absl//absl/hash:hash_testing",
    ],
)

cc_library(
    name = "type",
    srcs = glob(
        [
            "types/*.cc",
        ],
        exclude = [
            "types/*_test.cc",
        ],
    ) + [
        "type_factory.cc",
        "type_manager.cc",
        "type_provider.cc",
    ],
    hdrs = glob(
        [
            "types/*.h",
        ],
        exclude = [
            "types/*_test.h",
        ],
    ) + [
        "type.h",
        "type_factory.h",
        "type_interface.h",
        "type_manager.h",
        "type_provider.h",
    ],
    deps = [
        ":casting",
        ":memory",
        ":native_type",
        ":sized_input_view",
        ":type_kind",
        "//common/internal:data_interface",
        "//internal:names",
        "//internal:no_destructor",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "type_test",
    srcs = glob([
        "types/*_test.cc",
    ]) + [
        "type_factory_test.cc",
        "type_test.cc",
    ],
    deps = [
        ":casting",
        ":memory",
        ":native_type",
        ":type",
        "//internal:testing",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "value",
    srcs = glob(
        [
            "values/*.cc",
        ],
        exclude = [
            "values/*_test.cc",
        ],
    ) + [
        "list_value_provider.cc",
        "map_value_provider.cc",
        "value_factory.cc",
        "value_interface.cc",
        "value_manager.cc",
        "value_provider.cc",
    ],
    hdrs = glob(
        [
            "values/*.h",
        ],
        exclude = [
            "values/*_test.h",
        ],
    ) + [
        "value.h",
        "value_factory.h",
        "value_interface.h",
        "value_manager.h",
        "value_provider.h",
    ],
    linkopts = ["-ldl"],
    deps = [
        ":any",
        ":casting",
        ":json",
        ":memory",
        ":native_type",
        ":type",
        ":type_kind",
        ":value_kind",
        "//common/internal:data_interface",
        "//common/internal:shared_byte_string",
        "//internal:deserialize",
        "//internal:no_destructor",
        "//internal:overflow",
        "//internal:overloaded",
        "//internal:serialize",
        "//internal:status_macros",
        "//internal:strings",
        "//internal:time",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "value_test",
    srcs = glob([
        "values/*_test.cc",
    ]) + [
        "value_factory_test.cc",
        "value_provider_test.cc",
        "value_test.cc",
    ],
    deps = [
        ":any",
        ":casting",
        ":json",
        ":memory",
        ":native_type",
        ":type",
        ":value",
        ":value_kind",
        ":value_testing",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:cord_test_helpers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "sized_input_view",
    hdrs = ["sized_input_view.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_check",
    ],
)

cc_test(
    name = "sized_input_view_test",
    srcs = ["sized_input_view_test.cc"],
    deps = [
        ":sized_input_view",
        "//internal:testing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "sized_input_view_benchmark_test",
    srcs = ["sized_input_view_benchmark_test.cc"],
    tags = ["benchmark"],
    deps = [
        ":sized_input_view",
        "//internal:benchmark",
        "//internal:testing",
        "@com_google_absl//absl/strings:string_view",
    ],
)
