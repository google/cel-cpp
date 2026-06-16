# Copyright 2026 Google LLC
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
This module contains build rules for generating policy conformance test targets.
"""

load("@rules_cc//cc:cc_test.bzl", "cc_test")

def cel_policy_conformance_test(name, test_files, example, skip_tests = [], **kwargs):
    """Generates a policy conformance test target.

    Args:
        name: Name of the test target.
        test_files: List of targets or files representing the test data.
        example: A specific example file from test_files used for runfiles resolution.
        skip_tests: List of test cases to skip.
        testdata_dir: Path to testdata directory under runfiles.
        **kwargs: Additional arguments passed to the underlying cc_test.
    """
    args = ["--gunit_fail_if_no_test_linked"]
    args.append("--testdata_example='$(rlocationpath {})'".format(example))

    if skip_tests:
        args.append("--skip_tests=" + ",".join(skip_tests))

    cc_test(
        name = name,
        data = test_files + [example],
        deps = [
            "//conformance/policy:policy_conformance_test_lib",
        ],
        args = args,
        **kwargs
    )
