load("@rules_java//java:defs.bzl", "java_binary")

package(default_visibility = ["//visibility:public"])

java_binary(
    name = "antlr4_tool",
    main_class = "org.antlr.v4.Tool",
    runtime_deps = ["@antlr4_jar//jar"],
)
