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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_SAFE_TYPE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_SAFE_TYPE_MANAGER_H_

#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"
#include "common/types/type_cache.h"

namespace cel::common_internal {

class ThreadSafeTypeManager : public virtual TypeManager {
 public:
  explicit ThreadSafeTypeManager(MemoryManagerRef memory_manager,
                                 Shared<TypeIntrospector> type_introspector)
      : memory_manager_(memory_manager),
        type_introspector_(std::move(type_introspector)) {}

  MemoryManagerRef GetMemoryManager() const final { return memory_manager_; }

 protected:
  TypeIntrospector& GetTypeIntrospector() const final {
    return *type_introspector_;
  }

 private:
  ListType CreateListTypeImpl(const Type& element) final;

  MapType CreateMapTypeImpl(const Type& key, const Type& value) final;

  StructType CreateStructTypeImpl(absl::string_view name) final;

  OpaqueType CreateOpaqueTypeImpl(absl::string_view name,
                                  absl::Span<const Type> parameters) final;

  MemoryManagerRef memory_manager_;
  Shared<TypeIntrospector> type_introspector_;
  absl::Mutex list_types_mutex_;
  ListTypeCacheMap list_types_ ABSL_GUARDED_BY(list_types_mutex_);
  absl::Mutex map_types_mutex_;
  MapTypeCacheMap map_types_ ABSL_GUARDED_BY(map_types_mutex_);
  absl::Mutex struct_types_mutex_;
  StructTypeCacheMap struct_types_ ABSL_GUARDED_BY(struct_types_mutex_);
  absl::Mutex opaque_types_mutex_;
  OpaqueTypeCacheMap opaque_types_ ABSL_GUARDED_BY(opaque_types_mutex_);
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_SAFE_TYPE_MANAGER_H_
