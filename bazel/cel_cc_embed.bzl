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
Provides the `cel_cc_embed` build rule.
"""

def _cel_cc_embed(ctx):
    output = ctx.actions.declare_file(ctx.attr.name + ".inc")
    args = ctx.actions.args()
    src = ctx.file.src
    args.add("--in", src)
    args.add("--out", output.path)
    ctx.actions.run(
        outputs = [output],
        inputs = [src],
        progress_message = "generating embed textual header",
        executable = ctx.executable.gen_tool,
        arguments = [args]
    )

    return DefaultInfo(files = depset([output]),
            runfiles = ctx.runfiles(files=[output]))


cel_cc_embed = rule(
    implementation = _cel_cc_embed,
    attrs = {
        "src": attr.label(allow_single_file=True, mandatory=True),
        "gen_tool": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//bazel:cel_cc_embed")
        )
    }
)