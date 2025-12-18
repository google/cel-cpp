// Copyright 2025 Google LLC
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

#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/log/initialize.h"

// Read a bazel param file and concatenate the inputs.
// The param file is line delimited with each line a file to concat.
int main(int argc, char** argv) {
  absl::InitializeLog();
  if (argc != 3) {
    std::cerr << "usage: cat_param_file <param_file> <out>" << std::endl;
    std::cerr << "args " << argc << std::endl;
    return 2;
  }

  const char* param_file = argv[1];
  const char* out_file = argv[2];
  std::ifstream ifs(param_file, std::ios::binary);
  std::ofstream ofs(out_file, std::ios::binary);

  ABSL_QCHECK(ifs.good()) << "failed to open param file " << param_file;
  ABSL_QCHECK(ofs.good()) << "failed to open out file " << out_file;

  for (std::string line; std::getline(ifs, line);) {
    std::ifstream in(line, std::ios::binary);
    if (!in.good()) {
      ABSL_LOG(ERROR) << "failed to open input file " << line;
      continue;
    }
    constexpr size_t kBufSize = 256;
    char buf[kBufSize];
    while (true) {
      in.read(buf, kBufSize);
      size_t read = in.gcount();
      if (read == 0) {
        break;
      }
      ofs.write(buf, read);
    }
  }

  ofs.flush();

  return 0;
}
