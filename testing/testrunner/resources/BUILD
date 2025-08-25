package(default_visibility = ["//visibility:public"])

exports_files(
    ["test.cel"],
    visibility = ["//testing/testrunner:__subpackages__"],
)

filegroup(
    name = "resources",
    srcs = glob([
        "*.textproto",
    ]),
    visibility = ["//testing/testrunner:__subpackages__"],
)
