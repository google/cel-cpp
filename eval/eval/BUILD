# This package contains implementation of expression evaluator
# internals.
package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "evaluator_core",
    srcs = [
        "evaluator_core.cc",
    ],
    hdrs = [
        "evaluator_core.h",
    ],
    deps = [
        ":attribute_trail",
        ":attribute_utility",
        ":evaluator_stack",
        "//base:data",
        "//base:handle",
        "//base:memory",
        "//internal:rtti",
        "//internal:status_macros",
        "//runtime:activation_interface",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "cel_expression_flat_impl",
    srcs = [
        "cel_expression_flat_impl.cc",
    ],
    hdrs = [
        "cel_expression_flat_impl.h",
    ],
    deps = [
        ":evaluator_core",
        "//base:data",
        "//eval/internal:adapter_activation_impl",
        "//eval/internal:interop",
        "//eval/public:cel_expression",
        "//extensions/protobuf:memory_manager",
    ],
)

cc_library(
    name = "evaluator_stack",
    srcs = [
        "evaluator_stack.cc",
    ],
    hdrs = [
        "evaluator_stack.h",
    ],
    deps = [
        ":attribute_trail",
        "//base:data",
        "//base:handle",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "evaluator_stack_test",
    srcs = [
        "evaluator_stack_test.cc",
    ],
    deps = [
        ":evaluator_stack",
        "//base:attributes",
        "//base:data",
        "//extensions/protobuf:memory_manager",
        "//internal:testing",
    ],
)

cc_library(
    name = "expression_step_base",
    hdrs = [
        "expression_step_base.h",
    ],
    deps = [":evaluator_core"],
)

cc_library(
    name = "const_value_step",
    srcs = [
        "const_value_step.cc",
    ],
    hdrs = [
        "const_value_step.h",
    ],
    deps = [
        ":compiler_constant_step",
        ":evaluator_core",
        ":expression_step_base",
        "//base:ast_internal",
        "//base:handle",
        "//base:value",
        "//eval/internal:interop",
        "//eval/public:cel_value",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "container_access_step",
    srcs = [
        "container_access_step.cc",
    ],
    hdrs = [
        "container_access_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:ast_internal",
        "//base:attributes",
        "//base:data",
        "//base:kind",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//internal:status_macros",
        "//runtime/internal:number",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "regex_match_step",
    srcs = ["regex_match_step.cc"],
    hdrs = ["regex_match_step.h"],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:value",
        "//eval/internal:interop",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "ident_step",
    srcs = [
        "ident_step.cc",
    ],
    hdrs = [
        "ident_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":evaluator_core",
        ":expression_step_base",
        "//base:ast_internal",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "function_step",
    srcs = [
        "function_step.cc",
    ],
    hdrs = [
        "function_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":evaluator_core",
        ":expression_step_base",
        "//base:ast_internal",
        "//base:data",
        "//base:function",
        "//base:function_descriptor",
        "//base:handle",
        "//base:kind",
        "//eval/internal:errors",
        "//internal:status_macros",
        "//runtime:activation_interface",
        "//runtime:function_overload_reference",
        "//runtime:function_provider",
        "//runtime:function_registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "select_step",
    srcs = [
        "select_step.cc",
    ],
    hdrs = [
        "select_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:ast_internal",
        "//base:data",
        "//base:handle",
        "//base:memory",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "create_list_step",
    srcs = [
        "create_list_step.cc",
    ],
    hdrs = [
        "create_list_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        ":mutable_list_impl",
        "//base:ast_internal",
        "//base:handle",
        "//eval/internal:interop",
        "//eval/public/containers:container_backed_list_impl",
        "//extensions/protobuf:memory_manager",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "create_struct_step",
    srcs = [
        "create_struct_step.cc",
    ],
    hdrs = [
        "create_struct_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:ast_internal",
        "//base:data",
        "//eval/internal:interop",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:legacy_type_adapter",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "jump_step",
    srcs = [
        "jump_step.cc",
    ],
    hdrs = [
        "jump_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:value",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "logic_step",
    srcs = [
        "logic_step.cc",
    ],
    hdrs = [
        "logic_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:builtins",
        "//base:data",
        "//base:handle",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "comprehension_step",
    srcs = [
        "comprehension_step.cc",
    ],
    hdrs = [
        "comprehension_step.h",
    ],
    deps = [
        ":attribute_trail",
        ":evaluator_core",
        ":expression_step_base",
        "//base:data",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "comprehension_step_test",
    size = "small",
    srcs = [
        "comprehension_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":comprehension_step",
        ":evaluator_core",
        ":ident_step",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public/structs:cel_proto_wrapper",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "evaluator_core_test",
    size = "small",
    srcs = [
        "evaluator_core_test.cc",
    ],
    deps = [
        ":attribute_trail",
        ":cel_expression_flat_impl",
        ":evaluator_core",
        "//base:data",
        "//eval/compiler:cel_expression_builder_flat_impl",
        "//eval/internal:interop",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//extensions/protobuf:memory_manager",
        "//internal:testing",
        "//runtime:activation",
        "//runtime:runtime_options",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "const_value_step_test",
    size = "small",
    srcs = [
        "const_value_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":evaluator_core",
        "//base:ast_internal",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//eval/public/testing:matchers",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "container_access_step_test",
    size = "small",
    srcs = [
        "container_access_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":container_access_step",
        ":evaluator_core",
        ":ident_step",
        "//base:builtins",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "regex_match_step_test",
    size = "small",
    srcs = [
        "regex_match_step_test.cc",
    ],
    deps = [
        ":regex_match_step",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_options",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "ident_step_test",
    size = "small",
    srcs = [
        "ident_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":evaluator_core",
        ":ident_step",
        "//base:data",
        "//eval/public:activation",
        "//internal:testing",
        "//runtime:runtime_options",
    ],
)

cc_test(
    name = "function_step_test",
    size = "small",
    srcs = [
        "function_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":evaluator_core",
        ":expression_build_warning",
        ":function_step",
        ":ident_step",
        "//base:ast_internal",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_function",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:portable_cel_function_adapter",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "logic_step_test",
    size = "small",
    srcs = [
        "logic_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":evaluator_core",
        ":ident_step",
        ":logic_step",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
    ],
)

cc_test(
    name = "select_step_test",
    size = "small",
    srcs = [
        "select_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":evaluator_core",
        ":ident_step",
        ":select_step",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:legacy_type_adapter",
        "//eval/public/structs:trivial_legacy_type_info",
        "//eval/public/testing:matchers",
        "//eval/testutil:test_extensions_cc_proto",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "create_list_step_test",
    size = "small",
    srcs = [
        "create_list_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":const_value_step",
        ":create_list_step",
        ":evaluator_core",
        ":ident_step",
        "//eval/public:activation",
        "//eval/public:cel_attribute",
        "//eval/public:unknown_attribute_set",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "create_struct_step_test",
    size = "small",
    srcs = [
        "create_struct_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":create_struct_step",
        ":evaluator_core",
        ":ident_step",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:cel_type_registry",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/structs:cel_proto_wrapper",
        "//eval/public/structs:proto_message_type_adapter",
        "//eval/public/structs:protobuf_descriptor_type_provider",
        "//eval/testutil:test_message_cc_proto",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "//testutil:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "expression_build_warning",
    srcs = [
        "expression_build_warning.cc",
    ],
    hdrs = [
        "expression_build_warning.h",
    ],
    deps = [
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "expression_build_warning_test",
    size = "small",
    srcs = [
        "expression_build_warning_test.cc",
    ],
    deps = [
        ":expression_build_warning",
        "//internal:testing",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "attribute_trail",
    srcs = ["attribute_trail.cc"],
    hdrs = ["attribute_trail.h"],
    deps = [
        "//base:attributes",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/utility",
    ],
)

cc_test(
    name = "attribute_trail_test",
    size = "small",
    srcs = [
        "attribute_trail_test.cc",
    ],
    deps = [
        ":attribute_trail",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//extensions/protobuf:memory_manager",
        "//internal:testing",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "attribute_utility",
    srcs = ["attribute_utility.cc"],
    hdrs = ["attribute_utility.h"],
    deps = [
        ":attribute_trail",
        "//base:attributes",
        "//base:data",
        "//base:function_descriptor",
        "//base:function_result_set",
        "//base:handle",
        "//base/internal:unknown_set",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "attribute_utility_test",
    size = "small",
    srcs = [
        "attribute_utility_test.cc",
    ],
    deps = [
        ":attribute_utility",
        "//base:attributes",
        "//base:data",
        "//eval/public:cel_attribute",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//extensions/protobuf:memory_manager",
        "//internal:testing",
    ],
)

cc_library(
    name = "ternary_step",
    srcs = [
        "ternary_step.cc",
    ],
    hdrs = [
        "ternary_step.h",
    ],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:handle",
        "//base:value",
        "//eval/internal:errors",
        "//eval/internal:interop",
        "//eval/public:cel_builtins",
        "//eval/public:cel_value",
        "//eval/public:unknown_attribute_set",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "ternary_step_test",
    size = "small",
    srcs = [
        "ternary_step_test.cc",
    ],
    deps = [
        ":cel_expression_flat_impl",
        ":evaluator_core",
        ":ident_step",
        ":ternary_step",
        "//base:data",
        "//eval/public:activation",
        "//eval/public:unknown_attribute_set",
        "//eval/public:unknown_set",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:runtime_options",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "shadowable_value_step",
    srcs = ["shadowable_value_step.cc"],
    hdrs = ["shadowable_value_step.h"],
    deps = [
        ":evaluator_core",
        ":expression_step_base",
        "//base:handle",
        "//base:value",
        "//eval/internal:interop",
        "//eval/public:cel_value",
        "//extensions/protobuf:memory_manager",
        "//internal:status_macros",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "mutable_list_impl",
    hdrs = ["mutable_list_impl.h"],
    deps = ["//eval/public:cel_value"],
)

cc_test(
    name = "shadowable_value_step_test",
    size = "small",
    srcs = ["shadowable_value_step_test.cc"],
    deps = [
        ":cel_expression_flat_impl",
        ":evaluator_core",
        ":shadowable_value_step",
        "//base:data",
        "//base:handle",
        "//eval/internal:interop",
        "//eval/public:activation",
        "//eval/public:cel_value",
        "//internal:status_macros",
        "//internal:testing",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "compiler_constant_step",
    srcs = ["compiler_constant_step.cc"],
    hdrs = ["compiler_constant_step.h"],
    deps = [
        ":expression_step_base",
        "//internal:rtti",
    ],
)

cc_test(
    name = "compiler_constant_step_test",
    srcs = ["compiler_constant_step_test.cc"],
    deps = [
        ":compiler_constant_step",
        ":evaluator_core",
        "//base:data",
        "//extensions/protobuf:memory_manager",
        "//internal:rtti",
        "//internal:status_macros",
        "//internal:testing",
        "//runtime:activation",
        "//runtime:runtime_options",
    ],
)
