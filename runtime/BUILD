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

package(
    # Under active development, not yet being released.
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "activation_interface",
    hdrs = ["activation_interface.h"],
    deps = [
        ":function_overload_reference",
        "//base:attributes",
        "//base:memory_manager",
        "//base:value",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "function_overload_reference",
    hdrs = ["function_overload_reference.h"],
    deps = [
        "//base:function_interface",
        "//base:functions",
    ],
)

cc_library(
    name = "function_provider",
    hdrs = ["function_provider.h"],
    deps = [
        ":activation_interface",
        ":function_overload_reference",
        "//base:functions",
        "@com_google_absl//absl/status:statusor",
    ],
)
