"""
Main dependencies of cel-cpp.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_jar")

def base_deps():
    """Base evaluator and test dependencies."""

    # 2022-09-08
    ABSL_SHA1 = "518984e432e0597fd4e66a9c52148e8dec2bb46a"
    ABSL_SHA256 = "97e721f8f2a49c507190821a76cdf1c8b659eb49728e6dcf527670f943b2ba60"
    http_archive(
        name = "com_google_absl",
        urls = ["https://github.com/abseil/abseil-cpp/archive/" + ABSL_SHA1 + ".zip"],
        strip_prefix = "abseil-cpp-" + ABSL_SHA1,
        sha256 = ABSL_SHA256,
        patches = ["//bazel:abseil.patch"],
        patch_args = ["-p1"],
    )

    # v1.11.0
    GOOGLETEST_SHA1 = "e2239ee6043f73722e7aa812a459f54a28552929"
    GOOGLETEST_SHA256 = "8daa1a71395892f7c1ec5f7cb5b099a02e606be720d62f1a6a98f8f8898ec826"
    http_archive(
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/" + GOOGLETEST_SHA1 + ".zip"],
        strip_prefix = "googletest-" + GOOGLETEST_SHA1,
        sha256 = GOOGLETEST_SHA256,
    )

    # v1.6.0
    BENCHMARK_SHA1 = "f91b6b42b1b9854772a90ae9501464a161707d1e"
    BENCHMARK_SHA256 = "00bd0837db9266c758a087cdf0831a0d3e337c6bb9e3fad75d2be4f9bf480d95"
    http_archive(
        name = "com_github_google_benchmark",
        urls = ["https://github.com/google/benchmark/archive/" + BENCHMARK_SHA1 + ".zip"],
        strip_prefix = "benchmark-" + BENCHMARK_SHA1,
        sha256 = BENCHMARK_SHA256,
    )

    # 2021-09-01
    RE2_SHA1 = "8e08f47b11b413302749c0d8b17a1c94777495d5"
    RE2_SHA256 = "d635a3353bb8ffc33b0779c97c1c9d6f2dbdda286106a73bbcf498f66edacd74"
    http_archive(
        name = "com_googlesource_code_re2",
        urls = ["https://github.com/google/re2/archive/" + RE2_SHA1 + ".zip"],
        strip_prefix = "re2-" + RE2_SHA1,
        sha256 = RE2_SHA256,
    )

    PROTOBUF_VERSION = "3.21.1"
    PROTOBUF_SHA = "a295dd3b9551d3e2749a9969583dea110c6cdcc39d02088f7c7bb1100077e081"
    http_archive(
        name = "com_google_protobuf",
        sha256 = PROTOBUF_SHA,
        strip_prefix = "protobuf-" + PROTOBUF_VERSION,
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v" + PROTOBUF_VERSION + ".tar.gz"],
    )

    GOOGLEAPIS_GIT_SHA = "f19049fdd8dfc8b6eba387f4ef6d1d8b4d0103e7"  # May 31, 2022
    GOOGLEAPIS_SHA = "cbda1073fe2eb3b7a5a41fd940a592cfe1861895580c13bf25066896f9e9bede"
    http_archive(
        name = "com_google_googleapis",
        sha256 = GOOGLEAPIS_SHA,
        strip_prefix = "googleapis-" + GOOGLEAPIS_GIT_SHA,
        urls = ["https://github.com/googleapis/googleapis/archive/" + GOOGLEAPIS_GIT_SHA + ".tar.gz"],
    )

def parser_deps():
    """ANTLR dependency for the parser."""

    # Apr 15, 2022
    ANTLR4_VERSION = "4.10.1"

    http_archive(
        name = "antlr4_runtimes",
        build_file_content = """
package(default_visibility = ["//visibility:public"])
cc_library(
    name = "cpp",
    srcs = glob(["runtime/Cpp/runtime/src/**/*.cpp"]),
    hdrs = glob(["runtime/Cpp/runtime/src/**/*.h"]),
    includes = ["runtime/Cpp/runtime/src"],
)
  """,
        sha256 = "a320568b738e42735946bebc5d9d333170e14a251c5734e8b852ad1502efa8a2",
        strip_prefix = "antlr4-" + ANTLR4_VERSION,
        urls = ["https://github.com/antlr/antlr4/archive/v" + ANTLR4_VERSION + ".tar.gz"],
    )
    http_jar(
        name = "antlr4_jar",
        urls = ["https://www.antlr.org/download/antlr-" + ANTLR4_VERSION + "-complete.jar"],
        sha256 = "41949d41f20d31d5b8277187735dd755108df52b38db6c865108d3382040f918",
    )

def flatbuffers_deps():
    """FlatBuffers support."""
    FLAT_BUFFERS_SHA = "a83caf5910644ba1c421c002ef68e42f21c15f9f"
    http_archive(
        name = "com_github_google_flatbuffers",
        sha256 = "b8efbc25721e76780752bad775a97c3f77a0250271e2db37fc747b20e8b0f24a",
        strip_prefix = "flatbuffers-" + FLAT_BUFFERS_SHA,
        url = "https://github.com/google/flatbuffers/archive/" + FLAT_BUFFERS_SHA + ".tar.gz",
    )

def cel_spec_deps():
    """CEL Spec conformance testing."""
    http_archive(
        name = "io_bazel_rules_go",
        sha256 = "207fad3e6689135c5d8713e5a17ba9d1290238f47b9ba545b63d9303406209c6",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.24.7/rules_go-v0.24.7.tar.gz",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.24.7/rules_go-v0.24.7.tar.gz",
        ],
    )

    http_archive(
        name = "bazel_gazelle",
        sha256 = "b85f48fa105c4403326e9525ad2b2cc437babaa6e15a3fc0b1dbab0ab064bc7c",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.22.2/bazel-gazelle-v0.22.2.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.21.2/bazel-gazelle-v0.22.2.tar.gz",
        ],
    )

    CEL_SPEC_GIT_SHA = "2cfa4f6a2dd7cb101459f6a286a4920c7322649f"  # 9/7/2022
    http_archive(
        name = "com_google_cel_spec",
        sha256 = "78bfc17821607919724b033f1ba6e636d0cdfe056363055f4ab7f46b19e6a184",
        strip_prefix = "cel-spec-" + CEL_SPEC_GIT_SHA,
        urls = ["https://github.com/google/cel-spec/archive/" + CEL_SPEC_GIT_SHA + ".zip"],
    )

def cel_cpp_deps():
    """All core dependencies of cel-cpp."""
    base_deps()
    parser_deps()
    flatbuffers_deps()
    cel_spec_deps()
