"""
Main dependencies of cel-cpp.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def base_deps():
    """Base evaluator and test dependencies."""
    http_archive(
        name = "com_google_absl",
        patches = ["//bazel:abseil.patch"],
        patch_args = ["-p1"],
        strip_prefix = "abseil-cpp-master",
        urls = ["https://github.com/abseil/abseil-cpp/archive/master.zip"],
    )

    http_archive(
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/master.zip"],
        strip_prefix = "googletest-master",
    )

    http_archive(
        name = "com_github_google_benchmark",
        urls = ["https://github.com/google/benchmark/archive/master.zip"],
        strip_prefix = "benchmark-master",
    )

    http_archive(
        name = "com_googlesource_code_re2",
        strip_prefix = "re2-master",
        urls = ["https://github.com/google/re2/archive/master.zip"],
    )

    PROTOBUF_VERSION = "3.14.0"
    http_archive(
        name = "com_google_protobuf",
        strip_prefix = "protobuf-" + PROTOBUF_VERSION,
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v" + PROTOBUF_VERSION + ".tar.gz"],
    )

    GOOGLEAPIS_GIT_SHA = "be480e391cc88a75cf2a81960ef79c80d5012068"  # Jul 24, 2019
    GOOGLEAPIS_SHA = "c1969e5b72eab6d9b6cfcff748e45ba57294aeea1d96fd04cd081995de0605c2"
    http_archive(
        name = "com_google_googleapis",
        sha256 = GOOGLEAPIS_SHA,
        strip_prefix = "googleapis-" + GOOGLEAPIS_GIT_SHA,
        urls = ["https://github.com/googleapis/googleapis/archive/" + GOOGLEAPIS_GIT_SHA + ".tar.gz"],
    )

def parser_deps():
    """ANTLR dependency for the parser."""
    http_archive(
        name = "rules_antlr",
        sha256 = "7249d1569293d9b239e23c65f6b4c81a07da921738bde0dfeb231ed98be40429",
        strip_prefix = "rules_antlr-3cc2f9502a54ceb7b79b37383316b23c4da66f9a",
        urls = ["https://github.com/marcohu/rules_antlr/archive/3cc2f9502a54ceb7b79b37383316b23c4da66f9a.tar.gz"],
    )

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
        sha256 = "46f5e1af5f4bd28ade55cb632f9a069656b31fc8c2408f9aa045f9b5f5caad64",
        strip_prefix = "antlr4-4.7.2",
        urls = ["https://github.com/antlr/antlr4/archive/4.7.2.tar.gz"],
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

    CEL_SPEC_GIT_SHA = "95fe21a64063d63482a4b1b3159c07b5b7b64d77"  # 11/23/2020
    http_archive(
        name = "com_google_cel_spec",
        strip_prefix = "cel-spec-" + CEL_SPEC_GIT_SHA,
        urls = ["https://github.com/google/cel-spec/archive/" + CEL_SPEC_GIT_SHA + ".zip"],
    )

def cel_cpp_deps():
    """All core dependencies of cel-cpp."""
    base_deps()
    parser_deps()
    flatbuffers_deps()
    cel_spec_deps()
