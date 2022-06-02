package(default_visibility = ["//visibility:public"])

load("@rules_java//java:defs.bzl", "java_binary")

java_binary(
    name = "antlr4_tool",
    runtime_deps = ["@antlr4_jar//jar"],
    main_class = "org.antlr.v4.Tool",
)
