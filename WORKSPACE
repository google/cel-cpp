load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-master",
    urls = ["https://github.com/abseil/abseil-cpp/archive/master.zip"],
)

http_archive(
    name = "zlib",
    build_file_content = """
cc_library(
    name = "zlib",
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

CEL_SPEC_SHA="dd75cc98926a52975d303c9a635f18ab0aa1f2b8"
http_archive(
    name = "com_google_cel_spec",
    strip_prefix = "cel-spec-" + CEL_SPEC_SHA,
    urls = ["https://github.com/google/cel-spec/archive/" + CEL_SPEC_SHA + ".zip"],
)

# Google RE2 (Regular Expression) C++ Library
http_archive(
    name = "com_googlesource_code_re2",
    strip_prefix = "re2-master",
    urls = ["https://github.com/google/re2/archive/master.zip"],
)

GOOGLEAPIS_SHA = "184ab77f4cee62332f8f9a689c70c9bea441f836"
http_archive(
    name = "com_google_googleapis",
    sha256 = "a3a8c83314e5a431473659cb342a11e5520c6de4790eee70633d578f278b1e73",
    strip_prefix = "googleapis-" + GOOGLEAPIS_SHA,
    urls = ["https://github.com/googleapis/googleapis/archive/" + GOOGLEAPIS_SHA + ".tar.gz"],
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
GRPC_SHA = "08fd59f039c7cf62614ab7741b3f34527af103c7" # v1.22.0
http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "9dbb44a934d87faa8482c911e294a9f843a6c04d3936df8be116b1241bf475d9",
    strip_prefix = "grpc-" + GRPC_SHA,
    urls = ["https://github.com/grpc/grpc/archive/" + GRPC_SHA + ".tar.gz"],
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
