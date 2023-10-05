"""
Main dependencies of cel-cpp.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_jar")

def base_deps():
    """Base evaluator and test dependencies."""

    # 2023-05-15
    ABSL_SHA1 = "3aa3377ef66e6388ed19fd7c862bf0dc3a3630e0"
    ABSL_SHA256 = "91b144618b8d608764b556d56eba07d4a6055429807e8d8fca0566cc5b66485e"
    http_archive(
        name = "com_google_absl",
        urls = ["https://github.com/abseil/abseil-cpp/archive/" + ABSL_SHA1 + ".zip"],
        strip_prefix = "abseil-cpp-" + ABSL_SHA1,
        sha256 = ABSL_SHA256,
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

    # 2022-02-18
    RE2_SHA1 = "f6834581a8913c03d087de1e5d5b479f8a870400"
    RE2_SHA256 = "ef7f29b79f9e3a8e4030ea2a0f71a66bd99aa0376fe641d86d47d6129c7f5aed"
    http_archive(
        name = "com_googlesource_code_re2",
        urls = ["https://github.com/google/re2/archive/" + RE2_SHA1 + ".zip"],
        strip_prefix = "re2-" + RE2_SHA1,
        sha256 = RE2_SHA256,
    )

    PROTOBUF_VERSION = "23.0"
    PROTOBUF_SHA = "b8faf8487cc364e5c2b47a9abd77512bc79a6389ea45392ca938ba7617eae877"
    http_archive(
        name = "com_google_protobuf",
        sha256 = PROTOBUF_SHA,
        strip_prefix = "protobuf-" + PROTOBUF_VERSION,
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v" + PROTOBUF_VERSION + ".tar.gz"],
    )

    GOOGLEAPIS_GIT_SHA = "07c27163ac591955d736f3057b1619ece66f5b99"  # May 26, 2023
    GOOGLEAPIS_SHA = "bd8e735d881fb829751ecb1a77038dda4a8d274c45490cb9fcf004583ee10571"
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
        sha256 = "099a9fb96a376ccbbb7d291ed4ecbdfd42f6bc822ab77ae6f1b5cb9e914e94fa",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.35.0/rules_go-v0.35.0.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.35.0/rules_go-v0.35.0.zip",
        ],
    )

    http_archive(
        name = "bazel_gazelle",
        sha256 = "62ca106be173579c0a167deb23358fdfe71ffa1e4cfdddf5582af26520f1c66f",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.23.0/bazel-gazelle-v0.23.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.23.0/bazel-gazelle-v0.23.0.tar.gz",
        ],
    )

    CEL_SPEC_GIT_SHA = "7eb4db1aa8cebecb71b2b33a0ced33b9ae5f4fdc"  # 10/05/2023
    http_archive(
        name = "com_google_cel_spec",
        sha256 = "30cd1ab2675c64e1a489c3c40d04086d3598ba05dee938ddb5295d67bbe71ac9",
        strip_prefix = "cel-spec-" + CEL_SPEC_GIT_SHA,
        urls = ["https://github.com/google/cel-spec/archive/" + CEL_SPEC_GIT_SHA + ".zip"],
    )

def cel_cpp_deps():
    """All core dependencies of cel-cpp."""
    base_deps()
    parser_deps()
    flatbuffers_deps()
    cel_spec_deps()
