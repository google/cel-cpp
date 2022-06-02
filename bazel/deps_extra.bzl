"""
Transitive dependencies.
"""

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

def cel_spec_deps_extra():
    """CEL Spec dependencies."""
    go_repository(
        name = "org_golang_google_genproto",
        build_file_proto_mode = "disable_global",
        commit = "62d171c70ae133bd47722027b62f8820407cf744",
        importpath = "google.golang.org/genproto",
    )

    go_repository(
        name = "org_golang_google_grpc",
        build_file_proto_mode = "disable_global",
        importpath = "google.golang.org/grpc",
        tag = "v1.33.2",
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
        sum = "h1:g61tztE5qeGQ89tm6NTjjM9VPIm088od1l6aSorWRWg=",
        version = "v0.3.0",
    )

    go_rules_dependencies()
    go_register_toolchains()
    gazelle_dependencies()

def cel_cpp_deps_extra():
    """All transitive dependencies."""
    protobuf_deps()
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        go = True,  # cel-spec requirement
    )
    cel_spec_deps_extra()
