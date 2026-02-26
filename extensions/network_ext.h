// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_NETWORK_EXT_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_NETWORK_EXT_H_

#include "absl/status/status.h"
#include "compiler/compiler.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"

namespace cel::extensions {

// Provides a CEL compiler library for network functions.
cel::CompilerLibrary NetworkCompilerLibrary();

// Registers network function overloads with the function registry.
absl::Status RegisterNetworkFunctions(cel::FunctionRegistry& registry,
                                      const cel::RuntimeOptions& options);

// Registers network types with the type registry.
absl::Status RegisterNetworkTypes(cel::TypeRegistry& registry,
                                  const cel::RuntimeOptions& options);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_NETWORK_EXT_H_
