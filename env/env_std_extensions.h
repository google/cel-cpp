// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_ENV_STD_EXTENSIONS_H_
#define THIRD_PARTY_CEL_CPP_ENV_STD_EXTENSIONS_H_

#include "env/env.h"

namespace cel {

// Registers the standard CEL extensions with the given environment. This makes
// them available, but does not enable them. See Env::Config for how to enable
// extensions.
//
// Extensions are registered under the following names:
//
// - cel.lib.ext.bindings (alias: "bindings")
// - cel.lib.ext.encoders (alias: "encoders")
// - cel.lib.ext.lists (alias: "lists")
// - cel.lib.ext.math (alias: "math")
// - optional
// - cel.lib.ext.protos (alias: "protos")
// - cel.lib.ext.sets (alias: "sets")
// - cel.lib.ext.strings (alias: "strings")
// - cel.lib.ext.comprev2 (alias: "two-var-comprehensions")
// - cel.lib.ext.regex  (alias: "regex")
void RegisterStandardExtensions(Env& env);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_ENV_STD_EXTENSIONS_H_
