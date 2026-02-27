// Copyright 2024 Google LLC
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

#include "compiler/optional.h"

#include "absl/status/status.h"
#include "checker/optional.h"
#include "compiler/compiler.h"
#include "parser/macro.h"
#include "parser/parser_interface.h"

namespace cel {

CompilerLibrary OptionalCompilerLibrary(int version) {
  CompilerLibrary library =
      CompilerLibrary::FromCheckerLibrary(OptionalCheckerLibrary(version));

  library.configure_parser = [version](ParserBuilder& builder) {
    builder.GetOptions().enable_optional_syntax = true;
    absl::Status status;
    status.Update(builder.AddMacro(OptMapMacro()));
    if (version == 0) {
      return status;
    }
    status.Update(builder.AddMacro(OptFlatMapMacro()));
    return status;
  };

  return library;
}

}  // namespace cel
