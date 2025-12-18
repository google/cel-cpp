# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Generate C++ parser and lexer from a grammar file.
"""

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

def antlr_cc_library(name, src, package):
    """Creates a C++ lexer and parser from a source grammar.
    Args:
      name: Base name for the lexer and the parser rules.
      src: source ANTLR grammar file
      package: The namespace for the generated code
    """
    generated = name + "_grammar"
    antlr_library(
        name = generated,
        src = src,
        package = package,
        shell = select(
            {
                "@platforms//os:windows": "PowerShell.exe",
                "//conditions:default": "bash",
            },
        ),
        genfiles_prefixed = select(
            {
                "@platforms//os:windows": False,
                "//conditions:default": True,
            },
        ),
    )
    cc_library(
        name = name + "_cc_parser",
        srcs = [generated],
        defines = [
            "ANTLR4CPP_STATIC",
        ],
        deps = [
            generated,
            "@antlr4-cpp-runtime//:antlr4-cpp-runtime",
        ],
        linkstatic = 1,
    )

def _antlr_library(ctx):
    output = ctx.actions.declare_directory(ctx.attr.name)

    antlr_args = ctx.actions.args()
    antlr_args.add("-Dlanguage=Cpp")
    antlr_args.add("-no-listener")
    antlr_args.add("-visitor")
    antlr_args.add("-o", output.path)
    antlr_args.add("-package", ctx.attr.package)
    antlr_args.add(ctx.file.src)

    # Strip ".g4" extension.
    basename = ctx.file.src.basename[:-3]

    suffixes = ["Lexer", "Parser", "BaseVisitor", "Visitor"]

    ctx.actions.run(
        mnemonic = "GenAntlr",
        arguments = [antlr_args],
        inputs = [ctx.file.src],
        outputs = [output],
        executable = ctx.executable._tool,
        progress_message = "Processing ANTLR grammar. -o " + output.path,
    )

    files = []
    for suffix in suffixes:
        header = ctx.actions.declare_file(basename + suffix + ".h")
        source = ctx.actions.declare_file(basename + suffix + ".cpp")
        prefix = ctx.file.src.path[:-3] if ctx.attr.genfiles_prefixed else basename
        generated = output.path + "/" + prefix + suffix

        executable = ctx.attr.shell

        ctx.actions.run(
            mnemonic = "CopyHeader" + suffix,
            inputs = [output],
            outputs = [header],
            executable = executable,
            arguments = [
                "-c",
                'cp "{generated}" "{out}"'.format(generated = generated + ".h", out = header.path),
            ],
        )
        ctx.actions.run(
            mnemonic = "CopySource" + suffix,
            inputs = [output],
            outputs = [source],
            executable = executable,
            arguments = [
                "-c",
                'cp "{generated}" "{out}"'.format(generated = generated + ".cpp", out = source.path),
            ],
        )

        files.append(header)
        files.append(source)

    compilation_context = cc_common.create_compilation_context(headers = depset(files))
    return [DefaultInfo(files = depset(files)), CcInfo(compilation_context = compilation_context)]

antlr_library = rule(
    implementation = _antlr_library,
    attrs = {
        "src": attr.label(allow_single_file = [".g4"], mandatory = True),
        "package": attr.string(),
        "_tool": attr.label(
            executable = True,
            cfg = "exec",  # buildifier: disable=attr-cfg
            default = Label("//bazel:antlr4_tool"),
        ),
        "shell": attr.string(
            mandatory = True,
        ),
        "genfiles_prefixed": attr.bool(
            mandatory = True,
        ),
    },
)
