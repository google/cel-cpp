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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_ENV_TESTING_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_ENV_TESTING_H_

#include <memory>

#include "absl/base/nullability.h"
#include "runtime/internal/runtime_env.h"

namespace cel::runtime_internal {

ABSL_NONNULL std::shared_ptr<RuntimeEnv> NewTestingRuntimeEnv();

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_ENV_TESTING_H_
