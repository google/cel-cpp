# Copyright 2024 Google LLC
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
This module contains build rules for generating the conformance test targets.
"""

# Converts the list of tests to skip from the format used by the original Go test runner to a single
# flag value where each test is separated by a comma. It also performs expansion, for example
# `foo/bar,baz` becomes two entries which are `foo/bar` and `foo/baz`.
def _expand_tests_to_skip(tests_to_skip):
    result = []
    for test_to_skip in tests_to_skip:
        comma = test_to_skip.find(",")
        if comma == -1:
            result.append(test_to_skip)
            continue
        slash = test_to_skip.rfind("/", 0, comma)
        if slash == -1:
            slash = 0
        else:
            slash = slash + 1
        for part in test_to_skip[slash:].split(","):
            result.append(test_to_skip[0:slash] + part)
    return result

def _conformance_test_name(name, modern, arena, optimize, recursive):
    return "{}_{}_{}_{}_{}".format(
        name,
        "modern" if modern else "legacy",
        "arena" if arena else "refcount",
        "optimized" if optimize else "unoptimized",
        "recursive" if recursive else "iterative",
    )

def _conformance_test_args(modern, arena, optimize, recursive, skip_tests, dashboard):
    args = []
    if modern:
        args.append("--modern")
    elif not arena:
        fail("arena must be true for legacy")
    if not modern or arena:
        args.append("--arena")
    if optimize:
        args.append("--opt")
    if recursive:
        args.append("--recursive")
    args.append("--skip_tests={}".format(",".join(_expand_tests_to_skip(skip_tests))))
    if dashboard:
        args.append("--dashboard")
    return args

def _conformance_test(name, data, modern, arena, optimize, recursive, skip_tests, tags, dashboard):
    native.cc_test(
        name = _conformance_test_name(name, modern, arena, optimize, recursive),
        args = _conformance_test_args(modern, arena, optimize, recursive, skip_tests, dashboard) + ["$(location " + test + ")" for test in data],
        data = data,
        deps = ["//conformance:run"],
        tags = tags,
    )

def gen_conformance_tests(name, data, dashboard = False, skip_tests_modern = [], skip_tests_legacy = [], tags = []):
    """Generates conformance tests.

    Args:
        name: prefix for all tests
        data: textproto targets describing conformance tests
        skip_tests_modern: tests to skip for the modern implementation
        skip_tests_legacy: tests to skip for the legacy implementation
        tags: tags added to the generated targets
        dashboard: enable dashboard mode
    """

    # Modern
    # _conformance_test(name = name, data = data, modern = True, arena = False, optimize = False, recursive = False, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)
    # _conformance_test(name = name, data = data, modern = True, arena = False, optimize = True, recursive = False, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)
    # _conformance_test(name = name, data = data, modern = True, arena = False, optimize = False, recursive = True, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)
    # _conformance_test(name = name, data = data, modern = True, arena = False, optimize = True, recursive = True, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)
    _conformance_test(name = name, data = data, modern = True, arena = True, optimize = False, recursive = False, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)
    _conformance_test(name = name, data = data, modern = True, arena = True, optimize = True, recursive = False, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)
    _conformance_test(name = name, data = data, modern = True, arena = True, optimize = False, recursive = True, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)
    _conformance_test(name = name, data = data, modern = True, arena = True, optimize = True, recursive = True, skip_tests = skip_tests_modern, tags = tags, dashboard = dashboard)

    # Legacy
    _conformance_test(name = name, data = data, modern = False, arena = True, optimize = False, recursive = False, skip_tests = skip_tests_legacy, tags = tags, dashboard = dashboard)
    _conformance_test(name = name, data = data, modern = False, arena = True, optimize = True, recursive = False, skip_tests = skip_tests_legacy, tags = tags, dashboard = dashboard)
    _conformance_test(name = name, data = data, modern = False, arena = True, optimize = False, recursive = True, skip_tests = skip_tests_legacy, tags = tags, dashboard = dashboard)
    _conformance_test(name = name, data = data, modern = False, arena = True, optimize = True, recursive = True, skip_tests = skip_tests_legacy, tags = tags, dashboard = dashboard)
