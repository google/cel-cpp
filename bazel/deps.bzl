"""
Main dependencies of cel-cpp.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_jar")

def parser_deps():
    """ANTLR dependency for the parser."""

    # Sept 4, 2023
    ANTLR4_VERSION = "4.13.1"

    http_archive(
        name = "antlr4_runtimes",
        build_file_content = """
package(default_visibility = ["//visibility:public"])
cc_library(
    name = "cpp",
    srcs = glob(["runtime/Cpp/runtime/src/**/*.cpp"]),
    hdrs = glob(["runtime/Cpp/runtime/src/**/*.h"]),
    defines = ["ANTLR4CPP_USING_ABSEIL"],
    includes = ["runtime/Cpp/runtime/src"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/synchronization",
    ],
)
  """,
        sha256 = "365ff6aec0b1612fb964a763ca73748d80e0b3379cbdd9f82d86333eb8ae4638",
        strip_prefix = "antlr4-" + ANTLR4_VERSION,
        urls = ["https://github.com/antlr/antlr4/archive/refs/tags/" + ANTLR4_VERSION + ".zip"],
    )
    http_jar(
        name = "antlr4_jar",
        urls = ["https://www.antlr.org/download/antlr-" + ANTLR4_VERSION + "-complete.jar"],
        sha256 = "bc13a9c57a8dd7d5196888211e5ede657cb64a3ce968608697e4f668251a8487",
    )
