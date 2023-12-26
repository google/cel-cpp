package(default_visibility = ["//visibility:public"])

cc_library(
    name = "proto_ext",
    srcs = ["proto_ext.cc"],
    hdrs = ["proto_ext.h"],
    deps = [
        "//parser:macro",
        "//parser:source_factory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_library(
    name = "math_ext",
    srcs = ["math_ext.cc"],
    hdrs = ["math_ext.h"],
    deps = [
        "//eval/public:cel_function_registry",
        "//eval/public:cel_number",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:portable_cel_function_adapter",
        "//internal:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "math_ext_macros",
    srcs = ["math_ext_macros.cc"],
    hdrs = ["math_ext_macros.h"],
    deps = [
        "//parser:macro",
        "//parser:source_factory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "math_ext_test",
    srcs = ["math_ext_test.cc"],
    deps = [
        ":math_ext",
        ":math_ext_macros",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_function",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "regex_functions",
    srcs = ["regex_functions.cc"],
    hdrs = ["regex_functions.h"],
    deps = [
        "//eval/public:cel_function",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:portable_cel_function_adapter",
        "//eval/public/containers:container_backed_map_impl",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "bindings_ext",
    srcs = ["bindings_ext.cc"],
    hdrs = ["bindings_ext.h"],
    deps = [
        "//parser:macro",
        "//parser:source_factory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "regex_functions_test",
    srcs = [
        "regex_functions_test.cc",
    ],
    deps = [
        ":regex_functions",
        "//eval/public:activation",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_function_registry",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_map_impl",
        "//eval/public/testing:matchers",
        "//internal:testing",
        "//parser",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "bindings_ext_test",
    srcs = ["bindings_ext_test.cc"],
    deps = [
        ":bindings_ext",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_function_adapter",
        "//eval/public:cel_options",
        "//internal:testing",
        "//parser",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
    ],
)

cc_test(
    name = "bindings_ext_benchmark_test",
    srcs = ["bindings_ext_benchmark_test.cc"],
    deps = [
        ":bindings_ext",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public/testing:matchers",
        "//internal:benchmark",
        "//internal:no_destructor",
        "//internal:testing",
        "//parser",
        "//parser:macro",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "select_optimization",
    srcs = ["select_optimization.cc"],
    hdrs = ["select_optimization.h"],
    deps = [
        "//base:attributes",
        "//base:builtins",
        "//base:data",
        "//base:handle",
        "//base:memory",
        "//base/ast_internal:ast_impl",
        "//base/ast_internal:expr",
        "//common:kind",
        "//eval/compiler:flat_expr_builder_extensions",
        "//eval/eval:attribute_trail",
        "//eval/eval:evaluator_core",
        "//eval/eval:expression_step_base",
        "//eval/public:ast_rewrite_native",
        "//eval/public:source_position_native",
        "//internal:overloaded",
        "//internal:status_macros",
        "//runtime:runtime_options",
        "//runtime/internal:errors",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "sets_functions",
    srcs = ["sets_functions.cc"],
    hdrs = ["sets_functions.h"],
    deps = [
        "//base:data",
        "//base:function_adapter",
        "//base:handle",
        "//internal:status_macros",
        "//runtime:function_registry",
        "//runtime:runtime_options",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "sets_functions_test",
    srcs = ["sets_functions_test.cc"],
    deps = [
        ":sets_functions",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_function_adapter",
        "//eval/public:cel_options",
        "//internal:testing",
        "//parser",
        "//runtime:runtime_options",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "sets_functions_benchmark_test",
    srcs = ["sets_functions_benchmark_test.cc"],
    tags = ["benchmark"],
    deps = [
        ":sets_functions",
        "//base:data",
        "//base:memory",
        "//eval/internal:interop",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public/containers:container_backed_list_impl",
        "//extensions/protobuf:memory_manager",
        "//internal:benchmark",
        "//internal:status_macros",
        "//internal:testing",
        "//parser",
        "//runtime:runtime_options",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
