// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_MEMORY_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_MEMORY_MANAGER_H_

#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "base/memory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {

// Returns an appropriate `MemoryManagerRef` wrapping `google::protobuf::Arena`. The
// lifetime of objects creating using the resulting `MemoryManagerRef` is tied
// to that of `google::protobuf::Arena`.
//
// IMPORTANT: Passing `nullptr` here will result in getting
// `MemoryManagerRef::ReferenceCounting()`.
MemoryManagerRef ProtoMemoryManagerRef(
    google::protobuf::Arena* arena ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Returns a `PoolingMemoryManager` implementation which uses `google::protobuf::Arena`.
MemoryManager ProtoMemoryManager();

// Gets the underlying `google::protobuf::Arena`. If `MemoryManager` was not created using
// either `ProtoMemoryManagerRef` or `ProtoMemoryManager`, this returns
// `nullptr`.
absl::Nullable<google::protobuf::Arena*> ProtoMemoryManagerArena(
    MemoryManager& memory_manager ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Gets the underlying `google::protobuf::Arena`. If `MemoryManager` was not created using
// either `ProtoMemoryManagerRef` or `ProtoMemoryManager`, this returns
// `nullptr`.
absl::Nullable<google::protobuf::Arena*> ProtoMemoryManagerArena(
    MemoryManagerRef memory_manager);

// Allocate and construct `T` using the `ProtoMemoryManager` provided as
// `memory_manager`. `memory_manager` must be `ProtoMemoryManager` or behavior
// is undefined. Unlike `MemoryManager::New`, this method supports arena-enabled
// messages.
template <typename T, typename... Args>
ABSL_MUST_USE_RESULT T* NewInProtoArena(MemoryManagerRef memory_manager,
                                        Args&&... args) {
  return google::protobuf::Arena::Create<T>(ProtoMemoryManagerArena(memory_manager),
                                  std::forward<Args>(args)...);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_MEMORY_MANAGER_H_
