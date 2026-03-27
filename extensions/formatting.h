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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_FORMATTING_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_FORMATTING_H_

#include "absl/status/status.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

struct StringsExtensionFormatOptions {
  // The maximum precision to permit for formatting floating-point numbers.
  int max_precision = 1000;
};

// Register extension functions for string formatting.
//
// This implements (string).format([args...]) in the strings extension. Most
// users should add these functions via `extensions/strings.h` instead.
absl::Status RegisterStringFormattingFunctions(
    FunctionRegistry& registry, const RuntimeOptions& options,
    StringsExtensionFormatOptions format_options = {});

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_FORMATTING_H_
