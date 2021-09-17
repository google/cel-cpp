# Description
#   Libraries for working with protobuffer messages and well-known types.
#
# Uses the namespace google::api::expr::protoutil.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(
    name = "type_registry",
    srcs = ["type_registry.cc"],
    hdrs = [
        "type_registry.h",
    ],
    deps = [
        "//common:macros",
        "//common:parent_ref",
        "//common:type",
        "//common:value",
        "//internal:cast",
        "//internal:map_impl",
        "//internal:proto_util",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "type_registry_test",
    srcs = ["type_registry_test.cc"],
    deps = [
        ":type_registry",
        "//base:testing",
        "//common:value",
        "//internal:ref_countable",
        "//testutil:util",
        "@com_google_googleapis//google/type:money_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
