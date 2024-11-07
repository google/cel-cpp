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

#include <utility>

#include "absl/status/status.h"
#include "checker/optional.h"
#include "checker/type_checker_builder.h"
#include "compiler/compiler.h"
#include "parser/parser_interface.h"

namespace cel {

CompilerLibrary OptionalCompilerLibrary() {
  CheckerLibrary checker_library = OptionalCheckerLibrary();
  return CompilerLibrary(
      std::move(checker_library.id),
      [](ParserBuilder& builder) {
        builder.GetOptions().enable_optional_syntax = true;
        return absl::OkStatus();
      },
      std::move(checker_library.configure));
}

}  // namespace cel
