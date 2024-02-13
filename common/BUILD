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
        "@com_google_absl//absl/base:nullability",
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
        "@com_google_absl//absl/strings:string_view",
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
        "//internal:proto_wire",
        "//internal:status_macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:overload",
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
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
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
        "//internal:unicode",
        "//internal:utf8",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/functional:overload",
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
        "type_introspector.cc",
        "type_manager.cc",
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
        "type_introspector.h",
        "type_manager.h",
    ],
    deps = [
        ":casting",
        ":memory",
        ":native_type",
        ":sized_input_view",
        ":type_kind",
        "//common/internal:data_interface",
        "//internal:names",
        "//internal:status_macros",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/meta:type_traits",
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
        "list_type_reflector.cc",
        "map_type_reflector.cc",
        "type_reflector.cc",
        "value.cc",
        "value_factory.cc",
        "value_interface.cc",
        "value_manager.cc",
    ],
    hdrs = glob(
        [
            "values/*.h",
        ],
        exclude = [
            "values/*_test.h",
        ],
    ) + [
        "type_reflector.h",
        "value.h",
        "value_factory.h",
        "value_interface.h",
        "value_manager.h",
    ],
    deps = [
        ":any",
        ":casting",
        ":json",
        ":memory",
        ":native_type",
        ":type",
        ":type_kind",
        ":unknown",
        ":value_kind",
        "//base:attributes",
        "//common/internal:arena_string",
        "//common/internal:data_interface",
        "//common/internal:reference_count",
        "//common/internal:shared_byte_string",
        "//internal:deserialize",
        "//internal:dynamic_loader",
        "//internal:number",
        "//internal:overflow",
        "//internal:serialize",
        "//internal:status_macros",
        "//internal:strings",
        "//internal:time",
        "//internal:utf8",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "value_test",
    srcs = glob([
        "values/*_test.cc",
    ]) + [
        "type_reflector_test.cc",
        "value_factory_test.cc",
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

cc_library(
    name = "unknown",
    hdrs = ["unknown.h"],
    deps = ["//base/internal:unknown_set"],
)

# List of dynamic symbols we need for interop with google::api::expr::runtime::CelValue.
_DYNAMIC_SYMBOLS = [
    "cel_common_internal_LegacyListValue_GetType",
    "cel_common_internal_LegacyListValue_DebugString",
    "cel_common_internal_LegacyListValue_GetSerializedSize",
    "cel_common_internal_LegacyListValue_SerializeTo",
    "cel_common_internal_LegacyListValue_ConvertToJsonArray",
    "cel_common_internal_LegacyListValue_IsEmpty",
    "cel_common_internal_LegacyListValue_Size",
    "cel_common_internal_LegacyListValue_Get",
    "cel_common_internal_LegacyListValue_ForEach",
    "cel_common_internal_LegacyListValue_NewIterator",
    "cel_common_internal_LegacyListValue_Contains",
    "cel_common_internal_LegacyMapValue_GetType",
    "cel_common_internal_LegacyMapValue_DebugString",
    "cel_common_internal_LegacyMapValue_GetSerializedSize",
    "cel_common_internal_LegacyMapValue_SerializeTo",
    "cel_common_internal_LegacyMapValue_ConvertToJsonObject",
    "cel_common_internal_LegacyMapValue_IsEmpty",
    "cel_common_internal_LegacyMapValue_Size",
    "cel_common_internal_LegacyMapValue_Find",
    "cel_common_internal_LegacyMapValue_Get",
    "cel_common_internal_LegacyMapValue_Has",
    "cel_common_internal_LegacyMapValue_ListKeys",
    "cel_common_internal_LegacyMapValue_ForEach",
    "cel_common_internal_LegacyMapValue_NewIterator",
    "cel_common_internal_LegacyStructValue_DebugString",
    "cel_common_internal_LegacyStructValue_GetSerializedSize",
    "cel_common_internal_LegacyStructValue_SerializeTo",
    "cel_common_internal_LegacyStructValue_GetType",
    "cel_common_internal_LegacyStructValue_GetTypeName",
    "cel_common_internal_LegacyStructValue_ConvertToJsonObject",
    "cel_common_internal_LegacyStructValue_GetFieldByName",
    "cel_common_internal_LegacyStructValue_GetFieldByNumber",
    "cel_common_internal_LegacyStructValue_HasFieldByName",
    "cel_common_internal_LegacyStructValue_HasFieldByNumber",
    "cel_common_internal_LegacyStructValue_Equal",
    "cel_common_internal_LegacyStructValue_IsZeroValue",
    "cel_common_internal_LegacyStructValue_ForEachField",
    "cel_common_internal_LegacyStructValue_Qualify",
    "cel_common_internal_LegacyTypeReflector_NewListValueBuilder",
    "cel_common_internal_LegacyTypeReflector_NewMapValueBuilder",
]

cc_library(
    name = "legacy_value",
    srcs = ["legacy_value.cc"],
    hdrs = ["legacy_value.h"],
    # For potential compatibility with Windows and macOS, we avoid using weak symbols. Instead we
    # use the dynamic linker to find the symbols at runtime. This only works if they are exported as
    # dynamic symbols. When static linking, our symbols are not exported as dynamic by default. We
    # workaround this by explicitly passing our symbols to the linker for exporting.
    linkopts = ["-Wl,--export-dynamic-symbol=%s" % dynamic_symbol for dynamic_symbol in _DYNAMIC_SYMBOLS],
    deps = [
        ":casting",
        ":json",
        ":kind",
        ":native_type",
        ":type",
        ":unknown",
        ":value",
        ":value_kind",
        "//base:attributes",
        "//base/internal:message_wrapper",
        "//common/internal:arena_string",
        "//eval/internal:cel_value_equal",
        "//eval/public:cel_value",
        "//eval/public:message_wrapper",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:legacy_type_info_apis",
        "//extensions/protobuf:memory_manager",
        "//internal:casts",
        "//internal:status_macros",
        "//internal:time",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
)
