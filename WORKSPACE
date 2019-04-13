load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-master",
    urls = ["https://github.com/abseil/abseil-cpp/archive/master.zip"],
)

http_archive(
    name = "bazel_skylib",
    strip_prefix = "bazel-skylib-0.6.0",
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/0.6.0.zip"],
)

http_archive(
    name = "com_github_madler_zlib",
    build_file_content = """
cc_library(
    name = "z",
    srcs = [
        "adler32.c",
        "compress.c",
        "crc32.c",
        "deflate.c",
        "infback.c",
        "inffast.c",
        "inflate.c",
        "inftrees.c",
        "trees.c",
        "uncompr.c",
        "zutil.c",
    ],
    hdrs = [
        "crc32.h",
        "deflate.h",
        "gzguts.h",
        "inffast.h",
        "inffixed.h",
        "inflate.h",
        "inftrees.h",
        "trees.h",
        "zconf.h",
        "zlib.h",
        "zutil.h",
    ],
    includes = [
        ".",
    ],
    linkstatic = 1,
    visibility = [
        "//visibility:public",
    ],
)""",
    sha256 = "6d4d6640ca3121620995ee255945161821218752b551a1a180f4215f7d124d45",
    strip_prefix = "zlib-cacf7f1d4e3d44d871b605da3b647f07d718623f",
    url = "https://github.com/madler/zlib/archive/cacf7f1d4e3d44d871b605da3b647f07d718623f.tar.gz",
)

bind(
    name = "zlib",
    actual = "@com_github_madler_zlib//:z",
)

http_archive(
    name = "com_google_protobuf",
    strip_prefix = "protobuf-3.7.0",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.7.0.zip"],
)

http_archive(
    name = "com_google_protobuf_javalite",
    strip_prefix = "protobuf-javalite",
    urls = ["https://github.com/google/protobuf/archive/javalite.zip"],
)

http_archive(
    name = "com_google_googletest",
    strip_prefix = "googletest-master",
    urls = ["https://github.com/google/googletest/archive/master.zip"],
)

http_archive(
    name = "com_google_googlebench",
    strip_prefix = "benchmark-master",
    urls = ["https://github.com/google/benchmark/archive/master.zip"],
)

http_archive(
    name = "com_google_cel_spec",
    strip_prefix = "cel-spec-master",
    urls = ["https://github.com/google/cel-spec/archive/master.zip"],
)

# Google RE2 (Regular Expression) C++ Library
http_archive(
    name = "com_google_re2",
    strip_prefix = "re2-master",
    urls = ["https://github.com/google/re2/archive/master.zip"],
)

# gflags
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "6e16c8bc91b1310a44f3965e616383dbda48f83e8c1eaa2370a215057b00cabe",
    strip_prefix = "gflags-77592648e3f3be87d6c7123eb81cbad75f9aef5a",
    urls = [
        "https://mirror.bazel.build/github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
        "https://github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
    ],
)

# glog
http_archive(
    name = "com_google_glog",
    sha256 = "1ee310e5d0a19b9d584a855000434bb724aa744745d5b8ab1855c85bff8a8e21",
    strip_prefix = "glog-028d37889a1e80e8a07da1b8945ac706259e5fd8",
    urls = [
        "https://mirror.bazel.build/github.com/google/glog/archive/028d37889a1e80e8a07da1b8945ac706259e5fd8.tar.gz",
        "https://github.com/google/glog/archive/028d37889a1e80e8a07da1b8945ac706259e5fd8.tar.gz",
    ],
)

# Required to use embedded BUILD.bazel file in googleapis/google/rpc
git_repository(
    name = "io_grpc_grpc_java",
    remote = "https://github.com/grpc/grpc-java.git",
    tag = "v1.13.1",
)

http_archive(
    name = "com_google_googleapis",
    build_file_content = """
cc_proto_library(
    name = 'cc_rpc_status',
    deps = ['//google/rpc:status_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_rpc_code',
    deps = ['//google/rpc:code_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_type_money',
    deps = ['//google/type:money_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_expr_v1alpha1',
    deps = ['//google/api/expr/v1alpha1:syntax_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_expr_v1beta1',
    deps = ['//google/api/expr/v1beta1:eval_proto'],
    visibility = ['//visibility:public'],
)
""",
    strip_prefix = "googleapis-9a02c5acecb43f38fae4fa52c6420f21c335b888",
    urls = ["https://github.com/googleapis/googleapis/archive/9a02c5acecb43f38fae4fa52c6420f21c335b888.tar.gz"],
)

http_archive(
    name = "io_bazel_rules_go",
    url = "https://github.com/bazelbuild/rules_go/releases/download/0.18.0/rules_go-0.18.0.tar.gz",
    sha256 = "301c8b39b0808c49f98895faa6aa8c92cbd605ab5ad4b6a3a652da33a1a2ba2e",
)

load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies", "go_register_toolchains")
go_rules_dependencies()
go_register_toolchains()

http_archive(
    name = "com_google_api_codegen",
    urls = ["https://github.com/googleapis/gapic-generator/archive/96c3c5a4c8397d4bd29a6abce861547a271383e1.zip"],
    strip_prefix = "gapic-generator-96c3c5a4c8397d4bd29a6abce861547a271383e1",
    sha256 = "c8ff36df84370c3a0ffe141ec70c3633be9b5f6cc9746b13c78677e9d5566915",
)

#
# java_gapic artifacts runtime dependencies (gax-java)
# @unused
# buildozer: disable=load
load("@com_google_api_codegen//rules_gapic/java:java_gapic_repositories.bzl", "java_gapic_repositories")
