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
    # Implementation details for the checker library.
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "type_checker_impl",
    srcs = ["type_checker_impl.cc"],
    hdrs = ["type_checker_impl.h"],
    deps = [
        "//checker:type_check_issue",
        "//checker:type_checker",
        "//checker:validation_result",
        "//common:ast",
        "//common:source",
        "@com_google_absl//absl/status:statusor",
    ],
)
