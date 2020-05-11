"""
Generate C++ parser and lexer from a grammar file.
"""

load("@rules_antlr//antlr:antlr4.bzl", "antlr", "headers", "sources")

def antlr_cc_library(name, src, listener = False, visitor = True):
    """Creates a C++ lexer and parser from a source grammar.

    Args:
      name: Base name for the lexer and the parser rules.
      src: source ANTLR grammar file
      listener: generate ANTLR listener (default: False)
      visitor: generate ANTLR visitor (default: True)
    """
    generated = name + "_grammar"
    antlr(
        name = generated,
        srcs = [src],
        language = "Cpp",
        listener = listener,
        visitor = visitor,
        package = generated,
    )

    headers(
        name = "headers",
        rule = ":" + generated,
    )

    sources(
        name = "sources",
        rule = ":" + generated,
    )

    native.cc_library(
        name = name + "_cc_parser",
        hdrs = [":headers"],
        srcs = [":sources"],
        includes = ["$(INCLUDES)"],
        deps = ["@antlr4_runtimes//:cpp"],
        toolchains = [":" + generated],
        # ANTLR runtime does not build with dynamic linking
        linkstatic = True,
        alwayslink = 1,
    )
