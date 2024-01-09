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

#include "common/type_provider.h"

#include "common/memory.h"
#include "common/types/thread_compatible_type_provider.h"
#include "common/types/thread_safe_type_provider.h"

namespace cel {

Shared<TypeProvider> NewThreadCompatibleTypeProvider(
    MemoryManagerRef memory_manager) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleTypeProvider>();
}

Shared<TypeProvider> NewThreadSafeTypeProvider(
    MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<common_internal::ThreadSafeTypeProvider>();
}

}  // namespace cel
