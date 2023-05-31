"""
Transitive dependencies.
"""

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

def cel_spec_deps_extra():
    """CEL Spec dependencies."""

    # Generated Google APIs protos for Golang 05/25/2023
    go_repository(
        name = "org_golang_google_genproto_googleapis_api",
        build_file_proto_mode = "disable_global",
        importpath = "google.golang.org/genproto/googleapis/api",
        sum = "h1:m8v1xLLLzMe1m5P+gCTF8nJB9epwZQUBERm20Oy1poQ=",
        version = "v0.0.0-20230525234035-dd9d682886f9",
    )

    # Generated Google APIs protos for Golang 05/25/2023
    go_repository(
        name = "org_golang_google_genproto_googleapis_rpc",
        build_file_proto_mode = "disable_global",
        importpath = "google.golang.org/genproto/googleapis/rpc",
        sum = "h1:0nDDozoAU19Qb2HwhXadU8OcsiO/09cnTqhUtq2MEOM=",
        version = "v0.0.0-20230525234030-28d5490b6b19",
    )

    go_repository(
        name = "org_golang_google_grpc",
        build_file_proto_mode = "disable_global",
        importpath = "google.golang.org/grpc",
        tag = "v1.49.0",
        build_directives = ["gazelle:resolve go google.golang.org/genproto/googleapis/rpc/status @org_golang_google_genproto_googleapis_rpc//status:status"],
    )

    go_repository(
        name = "org_golang_x_net",
        importpath = "golang.org/x/net",
        sum = "h1:oWX7TPOiFAMXLq8o0ikBYfCJVlRHBcsciT5bXOrH628=",
        version = "v0.0.0-20190311183353-d8887717615a",
    )

    go_repository(
        name = "org_golang_x_text",
        importpath = "golang.org/x/text",
        sum = "h1:olpwvP2KacW1ZWvsR7uQhoyTYvKAupfQrRGBFM352Gk=",
        version = "v0.3.7",
    )

    go_rules_dependencies()
    go_register_toolchains(version = "1.19.1")
    gazelle_dependencies()
    protobuf_deps()

def cel_cpp_deps_extra():
    """All transitive dependencies."""
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        go = True,  # cel-spec requirement
    )
    cel_spec_deps_extra()
