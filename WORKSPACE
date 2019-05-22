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

CEL_SPEC_SHA="9cdb3682ba04109d2e03d9b048986bae113bf36f"
http_archive(
    name = "com_google_cel_spec",
    strip_prefix = "cel-spec-" + CEL_SPEC_SHA,
    urls = ["https://github.com/google/cel-spec/archive/" + CEL_SPEC_SHA + ".zip"],
    sha256 = "3172c95fc80d200948beb790806f881530c4e639469fa054eb8b6286cbbec7d3",
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
    name = 'cc_eval_v1alpha1',
    deps = ['//google/api/expr/v1alpha1:eval_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_checked_v1alpha1',
    deps = ['//google/api/expr/v1alpha1:checked_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_expr_v1beta1',
    deps = ['//google/api/expr/v1beta1:eval_proto'],
    visibility = ['//visibility:public'],
)

# gRPC dependencies
load("@com_github_grpc_grpc//bazel:generate_cc.bzl", "generate_cc")
generate_cc(
    name = "_conformance_service_proto",
    srcs = [
        "//google/api/expr/v1alpha1:conformance_service_proto",
    ],
    well_known_protos = True,
)
generate_cc(
    name = "_conformance_service_grpc",
    srcs = [
        "//google/api/expr/v1alpha1:conformance_service_proto",
    ],
    well_known_protos = True,
    plugin = "@com_github_grpc_grpc//:grpc_cpp_plugin",
)
cc_library(
    name = "cc_conformance_service",
    srcs = ["_conformance_service_proto", "_conformance_service_grpc"],
    hdrs = ["_conformance_service_proto", "_conformance_service_grpc"],
    deps = [
        ":cc_checked_v1alpha1",
        ":cc_expr_v1alpha1",
        ":cc_eval_v1alpha1",
        "@com_github_grpc_grpc//:grpc++_codegen_proto",
        "//external:protobuf",
    ],
    visibility = ['//visibility:public'],
)
""",
    strip_prefix = "googleapis-9a02c5acecb43f38fae4fa52c6420f21c335b888",
    urls = ["https://github.com/googleapis/googleapis/archive/9a02c5acecb43f38fae4fa52c6420f21c335b888.tar.gz"],
)

http_archive(
    name = "io_bazel_rules_go",
    url = "https://github.com/bazelbuild/rules_go/releases/download/0.18.5/rules_go-0.18.5.tar.gz",
    sha256 = "a82a352bffae6bee4e95f68a8d80a70e87f42c4741e6a448bec11998fcc82329",
)

load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies", "go_register_toolchains")

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

# cel-go dependencies:
http_archive(
    name = "bazel_gazelle",
    urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.17.0/bazel-gazelle-0.17.0.tar.gz"],
    sha256 = "3c681998538231a2d24d0c07ed5a7658cb72bfb5fd4bf9911157c0e9ac6a2687",
)
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
git_repository(
  name = "com_google_cel_go",
  remote = "https://github.com/google/cel-go.git",
  tag = "v0.2.0",
)
go_repository(
  name = "org_golang_google_genproto",
  build_file_proto_mode = "disable",
  commit = "bd91e49a0898e27abb88c339b432fa53d7497ac0",
  importpath = "google.golang.org/genproto",
)
go_repository(
  name = "com_github_antlr",
  tag = "4.7.2",
  importpath = "github.com/antlr/antlr4",
)
go_rules_dependencies()
go_register_toolchains()
gazelle_dependencies()

# gRPC dependencies:
git_repository(
    name = "com_github_grpc_grpc",
    remote = "https://github.com/grpc/grpc.git",
    commit = "7741e806a213cba63c96234f16d712a8aa101a49", # v1.20.1
    shallow_since = "1556224604 -0700",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
