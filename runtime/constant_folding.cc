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

#include "runtime/constant_folding.h"

#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/memory.h"
#include "eval/compiler/constant_folding.h"
#include "internal/casts.h"
#include "internal/rtti.h"
#include "internal/status_macros.h"
#include "runtime/internal/runtime_friend_access.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"

namespace cel::extensions {
namespace {

using ::cel::internal::down_cast;
using ::cel::internal::TypeId;
using ::cel::runtime_internal::RuntimeFriendAccess;
using ::cel::runtime_internal::RuntimeImpl;

absl::StatusOr<RuntimeImpl*> RuntimeImplFromBuilder(RuntimeBuilder& builder) {
  Runtime& runtime = RuntimeFriendAccess::GetMutableRuntime(builder);

  if (RuntimeFriendAccess::RuntimeTypeId(runtime) != TypeId<RuntimeImpl>()) {
    return absl::UnimplementedError(
        "constant folding only supported on the default cel::Runtime "
        "implementation.");
  }

  RuntimeImpl& runtime_impl = down_cast<RuntimeImpl&>(runtime);

  return &runtime_impl;
}

}  // namespace

absl::Status EnableConstantFolding(RuntimeBuilder& builder,
                                   MemoryManager& memory_manager) {
  CEL_ASSIGN_OR_RETURN(RuntimeImpl * runtime_impl,
                       RuntimeImplFromBuilder(builder));
  ABSL_ASSERT(runtime_impl != nullptr);
  runtime_impl->expr_builder().AddProgramOptimizer(
      runtime_internal::CreateConstantFoldingOptimizer(memory_manager));
  return absl::OkStatus();
}

}  // namespace cel::extensions
