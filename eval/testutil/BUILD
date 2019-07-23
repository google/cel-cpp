# This package contains testing utility code
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

proto_library(
    name = "test_message_protos",
    srcs = [
        "test_message.proto",
    ],
    deps = [
    ],
)

cc_proto_library(
    name = "test_message_cc_proto",
    deps = [":test_message_protos"],
)
