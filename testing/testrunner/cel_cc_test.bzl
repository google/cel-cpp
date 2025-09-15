# Copyright 2025 Google LLC
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

"""Rules for triggering the cc impl of the CEL test runner."""

load("@rules_cc//cc:cc_test.bzl", "cc_test")

def cel_cc_test(
        name,
        test_suite = "",
        cel_expr = "",
        is_raw_expr = False,
        filegroup = "",
        deps = [],
        test_data_path = "",
        data = []):
    """trigger the cc impl of the CEL test runner.

    This rule will generate a cc_test rule. This rule will be used to trigger
    the cc impl of the cel_test rule.

    Args:
        name: str name for the generated artifact
        test_suite: str label of a file containing a test suite. The file should have a
          .textproto extension.
        cel_expr: The CEL expression source. The meaning of this argument depends on `is_raw_expr`.
        is_raw_expr: bool whether the cel_expr is a raw expression string. If False,
          cel_expr is treated as a file path. The file type (.cel or .textproto)
          is inferred from the extension.
        filegroup: str label of a filegroup containing the test suite, the config and the checked
          expression.
        deps: list of dependencies for the cc_test rule.
        data: list of data dependencies for the cc_test rule.
        test_data_path: absolute path of the directory containing the test files. This is needed only
          if the test files are not located in the same directory as the BUILD file.
    """
    data, test_data_path = _update_data_with_test_files(
        data,
        filegroup,
        test_data_path,
        test_suite,
        cel_expr,
        is_raw_expr,
    )
    args = []

    test_data_path = test_data_path.lstrip("/")

    if test_suite != "":
        test_suite = test_data_path + "/" + test_suite
        args.append("--test_suite_path=" + test_suite)

    if cel_expr != "":
        expression_kind = ""
        cel_expr_value = ""
        if is_raw_expr:
            expression_kind = "raw"
            cel_expr_value = "\"" + cel_expr + "\""
        else:
            _, ext = _split_extension(cel_expr)
            resolved_path = test_data_path + "/" + cel_expr
            if ext == ".cel":
                expression_kind = "file"
            else:
                expression_kind = "checked"
            cel_expr_value = resolved_path

        args.append("--expression_kind=" + expression_kind)
        args.append("--cel_expr_value=" + cel_expr_value)

    cc_test(
        name = name,
        data = data,
        args = args,
        deps = ["//testing/testrunner:runner"] + deps,
    )

def _split_extension(path):
    """Extracts the file extension from a path string."""

    parts = path.rsplit(".", 1)
    if len(parts) == 1:
        return path, ""
    return parts[0], "." + parts[1]

def _update_data_with_test_files(data, filegroup, test_data_path, test_suite, cel_expr, is_raw_expr):
    """Updates the data with the test files."""

    if filegroup != "":
        data = data + [filegroup]
    elif test_data_path != "" and test_data_path != native.package_name():
        if test_suite != "":
            data = data + [test_data_path + ":" + test_suite]
        if cel_expr != "" and not is_raw_expr:
            data = data + [test_data_path + ":" + cel_expr]
    else:
        test_data_path = native.package_name()
        if test_suite != "":
            data = data + [test_suite]
        if cel_expr != "" and not is_raw_expr:
            data = data + [cel_expr]
    return data, test_data_path
