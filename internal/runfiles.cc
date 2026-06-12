// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/runfiles.h"

#include <string>

#include <fstream>

#include "rules_cc/cc/runfiles/runfiles.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace cel::internal {

std::string ResolveRunfilesPath(absl::string_view path) {
  using ::rules_cc::cc::runfiles::Runfiles;
  static Runfiles* runfiles = []() {
    std::string error;
    auto runfiles =
        Runfiles::CreateForTest(BAZEL_CURRENT_REPOSITORY, &error);
    ABSL_QCHECK(runfiles != nullptr)
        << absl::StrCat("failed to init runfiles", error);
    return runfiles;
  }();
  return runfiles->Rlocation(std::string(path));
}

absl::Status GetFileContents(absl::string_view path, std::string* out) {
  std::ifstream file{std::string(path)};
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Failed to open file: ", path));
  }
  out->append((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  return absl::OkStatus();
}

}  // namespace cel::internal
