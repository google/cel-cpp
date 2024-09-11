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
package(
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "type_check_issue",
    srcs = ["type_check_issue.cc"],
    hdrs = ["type_check_issue.h"],
    deps = [
        "//common:source",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "type_check_issue_test",
    srcs = ["type_check_issue_test.cc"],
    deps = [
        ":type_check_issue",
        "//common:source",
        "//internal:status_macros",
        "//internal:testing",
    ],
)

cc_library(
    name = "validation_result",
    hdrs = ["validation_result.h"],
    deps = [
        ":type_check_issue",
        "//common:ast",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "validation_result_test",
    srcs = ["validation_result_test.cc"],
    deps = [
        ":type_check_issue",
        ":validation_result",
        "//base/ast_internal:ast_impl",
        "//internal:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
    ],
)

cc_library(
    name = "type_checker",
    hdrs = ["type_checker.h"],
    deps = [
        ":validation_result",
        "//common:ast",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "type_checker_builder",
    srcs = ["type_checker_builder.cc"],
    hdrs = ["type_checker_builder.h"],
    deps = [
        ":type_checker",
        "//checker/internal:type_checker_impl",
        "@com_google_absl//absl/status:statusor",
    ],
)
