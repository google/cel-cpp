// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_FACTORY_H_

#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"

namespace cel {

// Create an unconfigured builder using the default Runtime implementation.
//
// This is provided for environments that only use a subset of the CEL standard
// builtins. Most users should prefer CreateStandardRuntimeBuilder.
//
// Callers must register appropriate builtins.
RuntimeBuilder CreateRuntimeBuilder(const RuntimeOptions& options);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_FACTORY_H_
