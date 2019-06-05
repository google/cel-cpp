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

GOOGLEAPIS_SHA = "4f3516a6f96dac182973a3573ff5117e8e4f76c7"
http_archive(
    name = "com_google_googleapis",
    strip_prefix = "googleapis-" + GOOGLEAPIS_SHA,
    urls = ["https://github.com/googleapis/googleapis/archive/" + GOOGLEAPIS_SHA + ".tar.gz"],
    sha256 = "5d185318b4e6b2675336ca2391d99d0adaae5ed88f721f6f02978b152cc8d29f",
)

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
    go = True,
    grpc = True,
)

http_archive(
    name = "io_bazel_rules_go",
    url = "https://github.com/bazelbuild/rules_go/releases/download/0.18.5/rules_go-0.18.5.tar.gz",
    sha256 = "a82a352bffae6bee4e95f68a8d80a70e87f42c4741e6a448bec11998fcc82329",
)

load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies", "go_register_toolchains")

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
GRPC_SHA = "7569ba7a66a2b4dd93b04ad355cc401d06285ee7" # v1.21.0
http_archive(
    name = "com_github_grpc_grpc",
    strip_prefix = "grpc-" + GRPC_SHA,
    urls = ["https://github.com/grpc/grpc/archive/" + GRPC_SHA + ".tar.gz"],
    sha256 = "e4864910b1127d2c05162df97986214874505e33cebfe6c9e36ca7eda4c6ad14",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
