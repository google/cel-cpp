# Copyright 2023 Google LLC
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
    default_testonly = True,
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "handle_matchers",
    hdrs = ["handle_matchers.h"],
    deps = ["//base:handle"],
)

cc_library(
    name = "kind_matchers",
    hdrs = ["kind_matchers.h"],
    deps = [
        ":handle_matchers",
        "//base:handle",
        "//base:kind",
        "//internal:testing",
    ],
)

cc_library(
    name = "type_matchers",
    hdrs = ["type_matchers.h"],
    deps = [
        ":handle_matchers",
        "//base:handle",
        "//base:type",
        "//internal:testing",
    ],
)

cc_library(
    name = "value_matchers",
    hdrs = ["value_matchers.h"],
    deps = [
        ":handle_matchers",
        "//base:handle",
        "//base:value",
        "//internal:testing",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
    ],
)
