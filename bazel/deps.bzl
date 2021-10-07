"""
Main dependencies of cel-cpp.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def base_deps():
    """Base evaluator and test dependencies."""

    # 2021-10-05
    ABSL_SHA1 = "b9b925341f9e90f5e7aa0cf23f036c29c7e454eb"
    ABSL_SHA256 = "bb2a0b57c92b6666e8acb00f4cbbfce6ddb87e83625fb851b0e78db581340617"
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

    PROTOBUF_VERSION = "3.18.0"
    PROTOBUF_SHA = "14e8042b5da37652c92ef6a2759e7d2979d295f60afd7767825e3de68c856c54"
    http_archive(
        name = "com_google_protobuf",
        sha256 = PROTOBUF_SHA,
        strip_prefix = "protobuf-" + PROTOBUF_VERSION,
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v" + PROTOBUF_VERSION + ".tar.gz"],
    )

    GOOGLEAPIS_GIT_SHA = "77066268d1fd5d72278afc2aef1ebc1d2112cca6"  # Oct 01, 2021
    GOOGLEAPIS_SHA = "dca75efd11a6295618dba919ad52fe551ba8bb85778d331a38c2bca282234296"
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

    CEL_SPEC_GIT_SHA = "c9ae91b24fdaf869d7c59a9f64863249a6a2905e"  # 9/22/2021
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
